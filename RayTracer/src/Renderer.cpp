#include "Renderer.h"

#include <Walnut/Image.h>
#include <Walnut/Random.h>

#include <glm/glm.hpp>

#include <ppl.h>
#include <execution>

#include "Ray.h"

#define DO_MT 1

using namespace Walnut;
using namespace glm;

Renderer::Renderer()
{
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_activeScene = &scene;
	m_activeCamera = &camera;

	uint32_t width = m_Image->GetWidth();
	uint32_t height = m_Image->GetHeight();

	float aspect = width / (float)height;

	if (m_frameindex == 1)
		memset(m_AccumulationBuffer, 0, width * height * sizeof(glm::vec4));

#if DO_MT
	//concurrency::parallel_for(uint32_t(0), height, [&](uint32_t y)
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),[this, width](uint32_t y)
#else
	for (uint32_t y = 0; y < height; y++)
#endif
	{
		for (uint32_t x = 0; x < width; x++)
		{
			int pixelIndex = y * width + x;
			glm::vec4 pixelColor = PerPixel(x, y);

			m_AccumulationBuffer[pixelIndex] += pixelColor;

			glm::vec4 accumulatedColor = m_AccumulationBuffer[pixelIndex] / (float) m_frameindex;
			accumulatedColor.a = 1.0f;

			m_ImageData[pixelIndex] = Util::ColorFromVec4(accumulatedColor);
		}
	}
#if DO_MT
	);
#endif

	if (m_accumulate)
		m_frameindex++;
	else
		m_frameindex = 1;


	m_Image->SetData(m_ImageData);
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_Image) {
		if (m_Image->GetWidth() == width && m_Image->GetHeight() == height)
			return;
		
		m_Image->Resize(width, height);
	}
	else {
		m_Image = std::make_shared<Walnut::Image>(width, height, ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	delete[] m_AccumulationBuffer;
	m_AccumulationBuffer = new glm::vec4[width * height];

	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;

	ResetFrameIndex();
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
	Ray ray;
	ray.Origin = m_activeCamera->GetPosition();
	ray.Direction = m_activeCamera->GetRayDirections()[y * m_Image->GetWidth() + x];



	glm::vec3 ambientColor(0.0f, 0.0f, 0.0f);

	glm::vec3 finalColor{ 0.0f };
	glm::vec3 contribution{ 1.0f };


	for (size_t i = 0; i < 5; i++)
	{
		// Shoot ray into scene
		HitData hitdata = TraceRay(ray);

		// no hit
		if (hitdata.Distance < 0.0f) {
			//finalColor += ambientColor * contribution;
			break;
		}

		glm::vec3 offsetPosition = hitdata.Position + hitdata.Normal * 0.0001f;

		glm::vec3 lightVec = m_lightPos - hitdata.Position;
		float lightDist = glm::length(lightVec);
		glm::vec3 lightDir = lightVec / lightDist;

		float lightIntensity = 1.0f / glm::pow(lightDist, m_lightPower);
		float diffuse = std::max(glm::dot(lightDir, hitdata.Normal) * lightIntensity, 0.0f);

		// Shadow?
		Ray shadowRay;
		shadowRay.Origin = offsetPosition;
		shadowRay.Direction = lightDir;
		HitData shadowhit = TraceRay(shadowRay);
		
		// Is in shadow
		if (shadowhit.Distance > 0.0f && shadowhit.Distance <= (lightDist - 0.0002f)) {
			diffuse *= 0.1f;
		}




		glm::vec3 hitColor = m_activeScene->spheres[hitdata.ObjectIndex].Color ;

		contribution *= hitColor;
		finalColor += contribution * hitColor * diffuse;



		ray.Origin = offsetPosition;
		ray.Direction = glm::normalize(hitdata.Normal + Random::InUnitSphere());

		//returnColor = glm::vec4(hitColor * diffuse, 1.0f);	
	}

	return glm::vec4(finalColor, 1.0f);

	//float closestDist = FLT_MAX;

	//for (size_t i = 0; i < m_activeScene->spheres.size(); i++)
	//{
	//	const Sphere& sphere = m_activeScene->spheres[i];

	//	glm::vec3 rayOrigin = ray.Origin - sphere.Position;

	//	float a = glm::dot(ray.Direction, ray.Direction);
	//	float b = 2.0f * glm::dot(rayOrigin, ray.Direction);
	//	float c = glm::dot(rayOrigin, rayOrigin) - sphere.Radius * sphere.Radius;

	//	float discriminant = b * b - 4.0f * a * c;


	//	// hit
	//	if (discriminant > 0.0f) {
	//		//float t0 = (-b + discriminant) / (2 * a);
	//		float t1 = (-b - glm::sqrt(discriminant)) / (2.0f * a);

	//		//t1 = std::min(t0, t1);

	//		if (t1 > closestDist || t1 < 0.0f)
	//			continue;

	//		closestDist = t1;

	//		glm::vec3 hitPoint = rayOrigin + ray.Direction * t1;
	//		glm::vec3 normal = glm::normalize(hitPoint);

	//		glm::vec3 lightVec = m_lightPos - hitPoint - sphere.Position;
	//		glm::vec3 lightDir = glm::normalize(lightVec);

	//		float lightDist = glm::length(lightVec);
	//		float lightIntensity = 1.0f / glm::pow(lightDist, m_lightPower);


	//		float diffuse = std::max(glm::dot(lightDir, normal), 0.0f);



	//		// Specular
	//		vec3 viewDir = normalize(hitPoint - rayOrigin);
	//		vec3 reflectDir = reflect(lightDir, normal);

	//		float spec = std::pow(std::max(dot(viewDir, reflectDir), 0.0f), 32.0f);
	//		spec = 0.0f;

	//		float specularStrength = 1.0f;
	//		vec3 specular = specularStrength * spec * vec3(1.0f);

	//		auto colorVec = lightIntensity * (diffuse + specular) * sphere.Color;


	//		//colorVec = normal * 0.5f + 0.5f;
	//		returnColor = glm::vec4(colorVec, 1.0f);
	//	}
	//}

	//return returnColor;
}

Renderer::HitData Renderer::TraceRay(const Ray& ray)
{
	HitData hitdata;
	hitdata.Distance = -1.0f;

	float closestDist = FLT_MAX;

	for (int i = 0; i < m_activeScene->spheres.size(); i++)
	{
		const Sphere& sphere = m_activeScene->spheres[i];

		glm::vec3 rayOrigin = ray.Origin - sphere.Position;

		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(rayOrigin, ray.Direction);
		float c = glm::dot(rayOrigin, rayOrigin) - sphere.Radius * sphere.Radius;

		float discriminant = b * b - 4.0f * a * c;

		// hit
		if (discriminant > 0.0f) {
			float t = (-b - glm::sqrt(discriminant)) / (2.0f * a);
			if (t > closestDist || t < 0.0f)
				continue;

			closestDist = t;

			glm::vec3 hitPoint = rayOrigin + ray.Direction * t;
			glm::vec3 normal = glm::normalize(hitPoint);

			// Set hit data
			hitdata.Distance = t;
			hitdata.Position = hitPoint + sphere.Position;
			hitdata.Normal = normal;
			hitdata.ObjectIndex = i;
		}
	}

	return hitdata;
}
