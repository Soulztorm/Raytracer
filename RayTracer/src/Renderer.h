#pragma once

#include <memory>
#include <vector>

#include <Walnut/Image.h>

#include "Scene.h"
#include "Camera.h"
#include "Ray.h"

float const Pi = std::atan(1.0f) * 4.0f;
float const TwoPi = 2.0f * Pi;

namespace Util {
	static uint32_t ColorFromVec4(glm::vec4 col) {
		glm::vec4 col_clamped = glm::clamp(col, glm::vec4(0.0f), glm::vec4(1.0f));

		uint8_t R = (uint8_t)(col_clamped.r * 255.0f);
		uint8_t G = (uint8_t)(col_clamped.g * 255.0f);
		uint8_t B = (uint8_t)(col_clamped.b * 255.0f);
		uint8_t A = (uint8_t)(col_clamped.a * 255.0f);

		return (A << 24) | (B << 16) | (G << 8) | R;
	}

	static glm::vec3 RandomHemisphere(const glm::vec3& normal, float spread)
	{
		// Make an orthogonal basis whose third vector is along `direction'
		glm::vec3 different = (std::abs(normal.x) < 0.5f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 b1 = (glm::cross(normal, different));
		glm::vec3 b2 = glm::cross(b1, normal);

		// Pick (x,y,z) randomly around (0,0,1)
		float z = Walnut::Random::Float(std::cos(spread * Pi), 1.0f);
		float r = std::sqrt(1.0f - z * z);
		float theta = Walnut::Random::Float(-Pi, +Pi);
		float x = r * std::cos(theta);
		float y = r * std::sin(theta);

		// Construct the vector that has coordinates (x,y,z) in the basis formed by b1, b2, b3
		return x * b1 + y * b2 + z * normal;
	}


	static glm::vec3 RandomUnitVector()
	{
		float z = Walnut::Random::Float() * 2.0f - 1.0f;
		float a = Walnut::Random::Float() * TwoPi;
		float r = sqrt(1.0f - z * z);
		float x = r * cos(a);
		float y = r * sin(a);
		return glm::vec3(x, y, z);
	}


	static const float ACE_a = 2.51f;
	static const float ACE_b = 0.03f;
	static const float ACE_c = 2.43f;
	static const float ACE_d = 0.59f;
	static const float ACE_e = 0.14f;

	static glm::vec3 ACESFilm(glm::vec3 x)
	{
		glm::vec3 aceColor = glm::clamp((x * (ACE_a * x + ACE_b)) / (x * (ACE_c * x + ACE_d) + ACE_e), 0.0f, 1.0f);

		return aceColor;
	}

	static glm::vec3 LessThan(glm::vec3 f, float value)
	{
		return glm::vec3(
			(f.x < value) ? 1.0f : 0.0f,
			(f.y < value) ? 1.0f : 0.0f,
			(f.z < value) ? 1.0f : 0.0f);
	}

	static glm::vec3 SRGBToLinear(glm::vec3 rgb)
	{
		rgb = glm::clamp(rgb, 0.0f, 1.0f);

		return glm::mix(
			glm::pow(((rgb + 0.055f) / 1.055f), glm::vec3(2.4f)),
			rgb / 12.92f,
			LessThan(rgb, 0.04045f)
		);
	}

	static glm::vec3 LinearToSRGB(glm::vec3 rgb)
	{
		rgb = clamp(rgb, 0.0f, 1.0f);

		return glm::mix(
			glm::pow(rgb, glm::vec3(1.0f / 2.4f)) * 1.055f - 0.055f,
			rgb * 12.92f,
			LessThan(rgb, 0.0031308f)
		);
	}
}

class Renderer {
public:
	struct Settings {
		bool Render = true;
		bool Accumulate = true;
		bool UseSphereScene = false;
		bool UseACE_Color = true;
		bool AntiAliasing = false;
		uint32_t Bounces = 8;
	};
	Settings& GetSettings() { return m_settings; }

	// Constructor
	Renderer();

	void Render(const Scene& scene, const Camera& camera);
	void OnResize(uint32_t width, uint32_t height);

	std::shared_ptr<Walnut::Image> GetImage() { return m_Image; }

	void ResetFrameIndex() { m_frameindex = 1; }
	uint32_t GetFrameIndex() { return m_frameindex; }


private:
	struct HitData {
		float Distance;
		glm::vec3 Position;
		glm::vec3 Normal;

		int MaterialIndex;
	};


	// Methods
	glm::vec3 PerPixel(uint32_t x, uint32_t y);
	HitData TraceRay(Ray* ray);

	HitData Miss();
	HitData ClosestHitSphere(Ray* ray, float distance, uint32_t objectIndex);
	HitData ClosestHitTriangle(Ray* ray, float distance, uint32_t objectIndex, float u, float v);

	bool RefractionRay(const glm::vec3& ray_dir_in, const glm::vec3& normal, const glm::vec3& intersection_point, float IOR, Ray& ray_out);

	bool IntersectRayTriangle(const Ray& ray, const Triangle& triangle, float& t);
	bool IntersectRayTriangle2(const Ray& ray, const Triangle& triangle, float& t);



	// Members
	const Camera* m_activeCamera = nullptr;
	const Scene* m_activeScene = nullptr;
	Settings m_settings = Settings();

	std::shared_ptr<Walnut::Image> m_Image;
	float* m_ImageData = nullptr;
	glm::vec3* m_AccumulationBuffer = nullptr;
	std::vector<uint32_t> m_ImageVerticalIter;

	uint32_t m_frameindex = 1;
};