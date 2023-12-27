#include "Renderer.h"

#include <Walnut/Image.h>
#include <Walnut/Random.h>

#include <glm/glm.hpp>

#include <ppl.h>
#include <execution>

#include "Ray.h"


using namespace Walnut;

const float EPSILON = 0.0002f;

Renderer::Renderer()
{
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_Image) {
		if (m_Image->GetWidth() == width && m_Image->GetHeight() == height)
			return;
		
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
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_activeScene = &scene;
	m_activeCamera = &camera;

	uint32_t width = m_Image->GetWidth();
	uint32_t height = m_Image->GetHeight();

	float aspect = width / (float)height;

	if (m_frameindex == 1)
		memset(m_AccumulationBuffer, 0, width * height * sizeof(glm::vec3));


	std::for_each(std::execution::par_unseq, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),[this, width](uint32_t y)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			uint32_t pixelIndex = y * width + x;
			glm::vec3 pixelColor = PerPixel(x, y);

			m_AccumulationBuffer[pixelIndex] += pixelColor;

			glm::vec3 accumulatedColor;
			if (m_settings.UseACE_Color)
				accumulatedColor = Util::LinearToSRGB(Util::ACESFilm(m_AccumulationBuffer[pixelIndex] / (float)m_frameindex));
			else
				accumulatedColor = m_AccumulationBuffer[pixelIndex] / (float)m_frameindex;


			uint32_t px = pixelIndex * 4;
			m_ImageData[px] = accumulatedColor.r;
			m_ImageData[px + 1] = accumulatedColor.g;
			m_ImageData[px + 2] = accumulatedColor.b;
			m_ImageData[px + 3] = 1.0f;
		}
	});

	// Anti alias
	if (m_settings.AntiAliasing) {
		int kernelSize = 1;
		std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(), [this, width, height, kernelSize](uint32_t y)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				glm::vec3 acc_px{ 0.0f };
				float weight = 0.0f;
				// Kernel
				for (int yo = -kernelSize; yo <= kernelSize; yo++)
				{
					for (int xo = -kernelSize; xo <= kernelSize; xo++)
					{
						// bounds check
						if (std::abs(xo + yo) > kernelSize || x + xo < 0 || x + xo >= width || y + yo < 0 || y + yo >= height)
							continue;

						float w = 1.0f - ((std::abs(yo) + std::abs(xo)) * 0.5f);
						acc_px += m_AccumulationBuffer[(y + yo) * width + x + xo] * w;
						weight += w;

					}
				}

				if (weight == 0.0f)
					continue;

				acc_px /= (weight * (float)m_frameindex);

				if (m_settings.UseACE_Color)
					acc_px = Util::LinearToSRGB(Util::ACESFilm(acc_px));


				int pixelIndex = 4 * (y * width + x);
				m_ImageData[pixelIndex] = acc_px.r;
				m_ImageData[pixelIndex+ 1] = acc_px.g;
				m_ImageData[pixelIndex+ 2] = acc_px.b;
				m_ImageData[pixelIndex+ 3] = 1.0f;
			}
		});
	}
	


	if (m_settings.Accumulate)
		m_frameindex++;
	else
		m_frameindex = 1;


	m_Image->SetData(m_ImageData);
}


glm::vec3 Renderer::PerPixel(uint32_t x, uint32_t y) {
	Ray ray;
	ray.Origin = m_activeCamera->GetPosition();
	ray.Direction = m_activeCamera->GetRayDirections()[y * m_Image->GetWidth() + x];


	glm::vec3 ambientColor{ 0.0f, 0.0f, 0.0f};
	glm::vec3 finalColor{ 0.0f };
	glm::vec3 contribution{ 1.0f };


	for (size_t i = 0; i < m_settings.Bounces; i++)
	{
		// Shoot ray into scene
		HitData hitdata = TraceRay(ray);

		// no hit
		if (hitdata.Distance < 0.0f) {
			finalColor += contribution * ambientColor;
			break;
		}

		// What material did we hit?
		Material mat = m_activeScene->materials[hitdata.MaterialIndex];




		bool doTransmission = false;
		bool hitInside = glm::dot(ray.Direction, hitdata.Normal) > 0.0f;
		glm::vec3 normalSurface = hitInside ? -hitdata.Normal : hitdata.Normal;

		// New ray origin offset from last hit position along surface normal
		ray.Origin = hitdata.Position + normalSurface * EPSILON;


		if (mat.Transparency > 0.0f) {
			// fresnel term    0: no reflect   1: full reflect
			float fresnel = glm::dot(ray.Direction, -normalSurface);

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
			if (RefractionRay(ray.Direction, hitdata.Normal, hitdata.Position, mat.IOR, refractionRay)) {
				ray = refractionRay;
				continue;
			}
		}
		else {
			glm::vec3 diffuseRayDir = glm::normalize(hitdata.Normal + Util::RandomUnitVector());
			glm::vec3 reflectedVector = glm::reflect(ray.Direction, hitdata.Normal);
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
			if (Random::Float() > p)
				break;

			// Add the energy we 'lose' by randomly terminating paths
			contribution *= 1.0f / p;
		}
	}

	return finalColor;
}

Renderer::HitData Renderer::TraceRay(const Ray& ray)
{
	float closestDistSpheres = FLT_MAX;
	int closestSphereIndex = -1;

	float closestDistTriangles = FLT_MAX;
	int closestTriangleIndex = -1;
	float closestTriangle_u = 0.0f;
	float closestTriangle_v = 0.0f;



	if (m_settings.UseSphereScene) {
		// Sphere intersections
		// Only dependant on ray direction
		float a = glm::dot(ray.Direction, ray.Direction);
		float dbl_a = (2.0f * a);

		// Loop through scene to find closest hit (if any)
		for (int i = 0; i < m_activeScene->spheres.size(); i++)
		{
			const Sphere& sphere = m_activeScene->spheres[i];

			glm::vec3 rayOrigin = ray.Origin - sphere.Position;

			float b = 2.0f * glm::dot(rayOrigin, ray.Direction);
			float c = glm::dot(rayOrigin, rayOrigin) - sphere.Radius * sphere.Radius;

			float discriminant = b * b - 4.0f * a * c;

			// hit
			if (discriminant >= 0.0f) {
				float t = (-b - glm::sqrt(discriminant)) / dbl_a;

				if (t > 0.0f && t < closestDistSpheres){
					closestDistSpheres = t;
					closestSphereIndex = i;
				}
			}
		}
	}
	else {
		// Triangle intersections
		float t = 0.0f;
		float u = 0.0f;
		float v = 0.0f;

		uint32_t hitIndex = 0;
		if (m_activeScene->kd_tree->intersect(ray.Origin, ray.Direction, t, hitIndex, u, v)) {
			closestDistTriangles = t;
			closestTriangleIndex = hitIndex;
			closestTriangle_u = u;
			closestTriangle_v = v;
		}
	}





	if (closestSphereIndex < 0 && closestTriangleIndex < 0)
		return Miss();

	// Sphere is closer
	if (closestDistSpheres < closestDistTriangles)
		return ClosestHitSphere(ray, closestDistSpheres, closestSphereIndex);

	// triangle closer
	return ClosestHitTriangle(ray, closestDistTriangles, closestTriangleIndex, closestTriangle_u, closestTriangle_v);
}

Renderer::HitData Renderer::ClosestHitSphere(const Ray& ray, float distance, uint32_t objectIndex)
{
	HitData hitdata;

	glm::vec3 hitPoint = ray.Origin + ray.Direction * distance;
	glm::vec3 normal = glm::normalize(hitPoint - m_activeScene->spheres[objectIndex].Position);

	// Set hit data
	hitdata.Distance = distance;
	hitdata.Position = hitPoint;
	hitdata.Normal = normal;
	hitdata.MaterialIndex = m_activeScene->spheres[objectIndex].MaterialIndex;

	return hitdata;
}

Renderer::HitData Renderer::ClosestHitTriangle(const Ray& ray, float distance, uint32_t objectIndex, float u, float v)
{
	glm::vec3 hitPoint = ray.Origin + ray.Direction * distance;
	glm::vec3 nrm = (1.0f - u - v) * m_activeScene->triangles[objectIndex].Normals[0] + u * m_activeScene->triangles[objectIndex].Normals[1] + v * m_activeScene->triangles[objectIndex].Normals[2];

	// Set hit data
	HitData hitdata;
	hitdata.Distance = distance;
	hitdata.Position = hitPoint;
	hitdata.Normal = glm::normalize(nrm);
	hitdata.MaterialIndex = m_activeScene->triangles[objectIndex].MaterialIndex;

	return hitdata;
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
		ray_out.Origin = intersection_point + (ref_n * -EPSILON);
		ray_out.Direction = glm::normalize((ray_dir_in + i_dot_n * ref_n) * eta - ref_n * std::sqrt(k));
		return true;
	}
}



Renderer::HitData Renderer::Miss()
{
	HitData hitdata;
	hitdata.Distance = -1.0f;

	return hitdata;
}




bool Renderer::IntersectRayTriangle(const Ray& ray, const Triangle& triangle, float& t)
{
	const glm::vec3& v0 = triangle.Vertices[0];
	const glm::vec3& v1 = triangle.Vertices[1];
	const glm::vec3& v2 = triangle.Vertices[2];

	// compute the plane's normal
	glm::vec3 v0v1 = v1 - v0;
	glm::vec3 v0v2 = v2 - v0;
	// no need to normalize
	glm::vec3 N = glm::cross(v0v1, v0v2); // N
	float area2 = (float) glm::length(N);

	// Step 1: finding P

	// check if the ray and plane are parallel.
	float NdotRayDirection = glm::dot(N, ray.Direction);
	if (fabs(NdotRayDirection) < 0.00001f) // almost 0
		return false; // they are parallel, so they don't intersect! 

	// compute d parameter using equation 2
	float d = -glm::dot(N, v0);

	// compute t (equation 3)
	t = -(glm::dot(N, ray.Origin) + d) / NdotRayDirection;

	// check if the triangle is behind the ray
	if (t < 0) return false; // the triangle is behind

	// compute the intersection point using equation 1
	glm::vec3 P = ray.Origin + t * ray.Direction;

	// Step 2: inside-outside test
	glm::vec3 C; // vector perpendicular to triangle's plane

	// edge 0
	glm::vec3 edge0 = v1 - v0;
	glm::vec3 vp0 = P - v0;
	C = glm::cross(edge0, vp0);
	if (glm::dot(N, C) < 0.0) return false; // P is on the right side

	// edge 1
	glm::vec3 edge1 = v2 - v1;
	glm::vec3 vp1 = P - v1;
	C = glm::cross(edge1, vp1);
	if (glm::dot(N, C) < 0.0)  return false; // P is on the right side

	// edge 2
	glm::vec3 edge2 = v0 - v2;
	glm::vec3 vp2 = P - v2;
	C = glm::cross(edge2, vp2);
	if (glm::dot(N, C) < 0.0) return false; // P is on the right side;

	return true; // this ray hits the triangle
}

bool Renderer::IntersectRayTriangle2(const Ray& ray, const Triangle& triangle, float& t)
{	
	constexpr float epsilon = std::numeric_limits<float>::epsilon();

	const glm::vec3& v0 = triangle.Vertices[0];
	const glm::vec3& v1 = triangle.Vertices[1];
	const glm::vec3& v2 = triangle.Vertices[2];

	glm::vec3 edge1 = v1 - v0;
	glm::vec3 edge2 = v2 - v0;
	glm::vec3 ray_cross_e2 = glm::cross(ray.Direction, edge2);
	float det = glm::dot(edge1, ray_cross_e2);

	if (det > -epsilon && det < epsilon)
		return false;    // This ray is parallel to this triangle.

	float inv_det = 1.0f / det;
	glm::vec3 s = ray.Origin - v0;
	float u = inv_det * glm::dot(s, ray_cross_e2);

	if (u < 0 || u > 1)
		return false;

	glm::vec3 ray_cross_e1 = glm::cross(s, edge1);
	float v = inv_det * glm::dot(ray.Direction, ray_cross_e1);

	if (v < 0 || u + v > 1)
		return false;

	// At this stage we can compute t to find out where the intersection point is on the line.
	float t0 = inv_det * glm::dot(edge2, ray_cross_e1);

	if (t0 > epsilon) // ray intersection
	{
		t = t0;
		return true;
	}
	else // This means that there is a line intersection but not a ray intersection.
		return false;
	
}
