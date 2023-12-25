#include "Renderer.h"

#include <Walnut/Image.h>
#include <Walnut/Random.h>

#include <glm/glm.hpp>

#include <ppl.h>
#include <execution>

#include "Ray.h"


using namespace Walnut;

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


	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),[this, width](uint32_t y)
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
	});

	if (m_settings.Accumulate)
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


	glm::vec3 finalColor{ 0.0f };
	glm::vec3 contribution{ 1.0f };


	for (size_t i = 0; i < m_settings.Bounces; i++)
	{
		// Shoot ray into scene
		HitData hitdata = TraceRay(ray);

		// no hit
		if (hitdata.Distance < 0.0f) {
			//finalColor += ambientColor * contribution;
			break;
		}

		glm::vec3 offsetPosition = hitdata.Position + hitdata.Normal * 0.0002f;


		Material mat = m_activeScene->materials[hitdata.MaterialIndex];


		//finalColor += contribution * hitColor * diffuse;
		contribution *= mat.Albedo;
		finalColor += mat.Emission * contribution;


		glm::vec3 directionToSurface = offsetPosition - ray.Origin;

		glm::vec3 randomHemisphereVector = glm::normalize(Util::RandomHemisphere(hitdata.Normal, mat.Roughness));
		glm::vec3 reflectedVector = glm::reflect(directionToSurface, randomHemisphereVector);
		//glm::vec3 reflectedVector = glm::reflect(directionToSurface, glm::normalize(hitdata.Normal + Random::InUnitSphere() * 0.9f * mat.Roughness));

		ray.Origin = offsetPosition;
		ray.Direction = (reflectedVector);
	}

	return glm::vec4(finalColor, 1.0f);
}

Renderer::HitData Renderer::TraceRay(const Ray& ray)
{
	float closestDistSpheres = FLT_MAX;
	int closestSphereIndex = -1;

	float closestDistTriangles = FLT_MAX;
	int closestTriangleIndex = -1;
	float closestTriangle_u = 0.0f;
	float closestTriangle_v = 0.0f;




	// Sphere intersections
	// Only dependant on ray direction
	//float a = glm::dot(ray.Direction, ray.Direction);
	//float dbl_a = (2.0f * a);

	// Loop through scene to find closest hit (if any)
	//for (int i = 0; i < m_activeScene->spheres.size(); i++)
	//{
	//	const Sphere& sphere = m_activeScene->spheres[i];

	//	glm::vec3 rayOrigin = ray.Origin - sphere.Position;

	//	float b = 2.0f * glm::dot(rayOrigin, ray.Direction);
	//	float c = glm::dot(rayOrigin, rayOrigin) - sphere.Radius * sphere.Radius;

	//	float discriminant = b * b - 4.0f * a * c;

	//	// hit
	//	if (discriminant >= 0.0f) {
	//		float t = (-b - glm::sqrt(discriminant)) / dbl_a;

	//		if (t > 0.0f && t < closestDistSpheres){
	//			closestDistSpheres = t;
	//			closestSphereIndex = i;
	//		}
	//	}
	//}




	// Triangle intersections
	//if (m_activeScene->kd_tree) {
		// We hit a triangle
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
		
	//}
	//else {
	//	for (int i = 0; i < m_activeScene->triangles.size(); i++)
	//	{
	//		const Triangle& triangle = m_activeScene->triangles[i];

	//		float t = 0.0f;

	//		if (IntersectRayTriangle2(ray, triangle, t)) {
	//			if (t < closestDistTriangles) {
	//				closestDistTriangles = t;
	//				closestTriangleIndex = i;
	//			}
	//		}
	//	}
	//}





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
	HitData hitdata;

	glm::vec3 hitPoint = ray.Origin + ray.Direction * distance;

	// Set hit data
	hitdata.Distance = distance;
	hitdata.Position = hitPoint;

	// (1-u-v) * p0 + u * p1 + v * p2

	glm::vec3 nrm = (1.0f - u - v) * m_activeScene->triangles[objectIndex].Normals[0] + u * m_activeScene->triangles[objectIndex].Normals[1] + v * m_activeScene->triangles[objectIndex].Normals[2];
	hitdata.Normal = glm::normalize(nrm);
	
	//hitdata.Normal = m_activeScene->triangles[objectIndex].Normal;


	hitdata.MaterialIndex = m_activeScene->triangles[objectIndex].MaterialIndex;

	return hitdata;
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
	float area2 = (float) N.length();

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
