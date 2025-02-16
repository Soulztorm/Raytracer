#include "Renderer.h"

#include <Walnut/Image.h>
#include <Walnut/Random.h>

#include <ppl.h>
#include <execution>

#include "Ray.h"

using namespace Walnut;


bool Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_Image) {
		if (m_Image->GetWidth() == width && m_Image->GetHeight() == height)
			return false;
		
		m_Image->Resize(width, height);
	}
	else {
		m_Image = std::make_shared<Walnut::Image>(width, height, ImageFormat::RGBA32F);
	}

	delete[] m_ImageData;
	m_ImageData = new float[4 * width * height];

	delete[] m_AccumulationBuffer;
	m_AccumulationBuffer = new glm::vec3[width * height];

	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;

	ResetFrameIndex();

	return true;
}

void Renderer::Render(Scene* scene, BVH* bvh, Camera* camera)
{
	m_activeScene = scene;
	m_activeBVH = bvh;
	m_activeCamera = camera;

	uint32_t width = m_Image->GetWidth();
	uint32_t height = m_Image->GetHeight();

	if (m_frameindex == 1)
		memset(m_AccumulationBuffer, 0, width * height * sizeof(glm::vec3));


	std::for_each(std::execution::par_unseq, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(), [this, width](uint32_t y)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			uint32_t pixelIndex = y * width + x;
			glm::vec3 pixelColor = PerPixel(x, y);

			m_AccumulationBuffer[pixelIndex] += pixelColor;

			glm::vec3 accumulatedColor = m_settings.Exposure * (m_AccumulationBuffer[pixelIndex] / (float)m_frameindex);
			if (m_settings.UseACE_Color)
				accumulatedColor = Util::LinearToSRGB(Util::ACESFilm(accumulatedColor));

			uint32_t px = pixelIndex * 4;
			m_ImageData[px] = accumulatedColor.r;
			m_ImageData[px + 1] = accumulatedColor.g;
			m_ImageData[px + 2] = accumulatedColor.b;
			m_ImageData[px + 3] = 1.0f;
		}
	});


	if (m_settings.Accumulate)
		m_frameindex++;
	else
		m_frameindex = 1;


	m_Image->SetData(m_ImageData);
}


glm::vec3 Renderer::PerPixel(uint32_t x, uint32_t y) {
	Ray ray;

	// Defocus ray origin
	glm::vec2 camJitter = Walnut::Random::InCircle() * m_settings.DoF_Strength;
	ray.Origin = m_activeCamera->GetPosition() + m_activeCamera->GetRight() * camJitter.x + m_activeCamera->GetUp() * camJitter.y;

	// Defocus at viewpoint
	glm::vec2 lookatJitter = Walnut::Random::InCircle() * m_settings.CamLookatJitter;
	glm::vec3 lookatVector = m_activeCamera->GetRayDirections()[y * m_Image->GetWidth() + x];
	glm::vec3 lookatPosition = lookatVector * m_settings.DoF_Distance + m_activeCamera->GetPosition() + m_activeCamera->GetRight() * lookatJitter.x + m_activeCamera->GetUp() * lookatJitter.y;
	ray.Direction = glm::normalize(lookatPosition - ray.Origin);


	glm::vec3 ambientColor{ 0.0f, 0.0f, 0.0f};
	glm::vec3 finalColor{ 0.0f };
	glm::vec3 contribution{ 1.0f };

	// Main Render mode
	if (m_settings.RenderMode == 0) {
		for (size_t i = 0; i < m_settings.Bounces; i++)
		{
			ray.DirectionInverse = glm::vec3(1.0f / ray.Direction.x, 1.0f / ray.Direction.y, 1.0f / ray.Direction.z);

			// Shoot ray into scene
			HitInfo hit = m_activeBVH->IntersectRay(&ray);

			// no hit
			if (hit.materialIndex < 0) {
				glm::vec3 skyColor = m_activeScene->hdri.GetPixelFromWorldDirection(ray.Direction);
				finalColor += contribution * skyColor;
				break;
			}

			// What material did we hit?
			Material mat = m_activeScene->materials[hit.materialIndex];




			bool doTransmission = false;
			//bool hitInside = glm::dot(ray.Direction, hitdata.Normal) > 0.0f;
			//glm::vec3 normalSurface = hitInside ? -hitdata.Normal : hitdata.Normal;

			// New ray origin offset from last hit position along surface normal
			ray.Origin = hit.position + hit.normal * FLT_EPSILON;


			if (mat.Transparency > 0.0f) {
				// fresnel term    0: no reflect   1: full reflect
				//float fresnel = glm::dot(ray.Direction, -normalSurface);
				float fresnel = glm::dot(ray.Direction, -hit.normal);

				//if (Walnut::Random::Float() < fresnel) {
				//if (Random::Float() < fresnel) {
				doTransmission = true;
				//}
			}


			// Transmission or reflection ray?
			if (doTransmission) {
				//// glsl way (me no workeee, why?)
				//ray.Direction = glm::refract(ray.Direction, hitdata.Normal, hitInside ? mat.IOR : 1.0f / mat.IOR);
				//continue;

				Ray refractionRay;
				if (RefractionRay(ray.Direction, hit.normal, hit.position, mat.IOR, refractionRay)) {
					ray = refractionRay;
					continue;
				}
			}
			else {
				//glm::vec3 diffuseRayDir = glm::normalize(hit.normal + Util::RandomUnitVector());
				glm::vec3 diffuseRayDir = glm::normalize(hit.normal + Walnut::Random::InUnitSphere());
				glm::vec3 reflectedVector = glm::reflect(ray.Direction, hit.normal);
				reflectedVector = glm::normalize(glm::mix(reflectedVector, diffuseRayDir, mat.Roughness * mat.Roughness));

				//glm::vec3 randomHemisphereVector = glm::normalize(Util::RandomHemisphere(hitdata.Normal, mat.Roughness));
				//glm::vec3 reflectedVector = glm::reflect(ray.Direction, randomHemisphereVector);

				ray.Direction = reflectedVector;
			}


			finalColor += mat.Emission * contribution;
			contribution *= mat.Albedo;


			// Russian Roulette
			// As the throughput gets smaller, the ray is more likely to get terminated early.
			// Survivors have their value boosted to make up for fewer samples being in the average.
			{
				float p = std::max(contribution.r, std::max(contribution.g, contribution.b));
				if (Walnut::Random::Float() > p)
					break;

				// Add the energy we 'lose' by randomly terminating paths
				contribution *= 1.0f / p;
			}
		}
	}

	// Debug view
	else {
		ray.DirectionInverse = glm::vec3(1.0f / ray.Direction.x, 1.0f / ray.Direction.y, 1.0f / ray.Direction.z);

		HitInfo hit = m_activeBVH->IntersectRay(&ray);
		if (hit.materialIndex >= 0) {
			finalColor = 0.5f * (hit.normal + glm::vec3(1.0f));
		}
	}

	return finalColor;
}


bool Renderer::RefractionRay(const glm::vec3& ray_dir_in, const glm::vec3& normal, const glm::vec3& intersection_point, float IOR, Ray& ray_out)
{
	glm::vec3 ref_n = normal;
	float eta_t = IOR;
	float eta_i = 1.0f;
	float i_dot_n = glm::dot(ray_dir_in, normal);

	if (i_dot_n < 0.0f) {
		i_dot_n = -i_dot_n;
	}
	else {
		//Inside the surface; invert the normal and swap the indices of refraction
		ref_n = -normal;
		eta_t = 1.0f;
		eta_i = IOR;
	}

	float eta = eta_i / eta_t;
	float k = 1.0f - (eta * eta) * (1.0f - i_dot_n * i_dot_n);
	if (k < 0.0f) {
		return false;
	}
	else {
		ray_out.Origin = intersection_point + (ref_n * -FLT_EPSILON);
		ray_out.Direction = glm::normalize((ray_dir_in + i_dot_n * ref_n) * eta - ref_n * std::sqrt(k));
		return true;
	}
}