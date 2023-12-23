#pragma once

#include <memory>
#include <vector>

#include <Walnut/Image.h>

#include <glm/glm.hpp>

#include "Scene.h"
#include "Camera.h"
#include "Ray.h"

namespace Util {
	static uint32_t ColorFromVec4(glm::vec4 col) {
		glm::vec4 col_clamped = glm::clamp(col, glm::vec4(0.0f), glm::vec4(1.0f));

		uint8_t R = (uint8_t)(col_clamped.r * 255.0f);
		uint8_t G = (uint8_t)(col_clamped.g * 255.0f);
		uint8_t B = (uint8_t)(col_clamped.b * 255.0f);
		uint8_t A = (uint8_t)(col_clamped.a * 255.0f);

		return (A << 24) | (B << 16) | (G << 8) | R;
	}
}

class Renderer {
public:
	struct Settings {
		bool Accumulate = true;
		uint32_t Bounces = 5;
	};
	Settings& GetSettings() { return m_settings; }

	// Constructor
	Renderer();

	void Render(const Scene& scene, const Camera& camera);
	void OnResize(uint32_t width, uint32_t height);

	std::shared_ptr<Walnut::Image> GetImage() { return m_Image; }

	void ResetFrameIndex() { m_frameindex = 1; }


private:
	struct HitData {
		float Distance;
		glm::vec3 Position;
		glm::vec3 Normal;

		int ObjectIndex;
	};


	// Methods
	glm::vec4 PerPixel(uint32_t x, uint32_t y);
	HitData TraceRay(const Ray& ray);

	HitData Miss();
	HitData ClosestHit(const Ray& ray, float distance, uint32_t hitIndex);


	// Members
	const Camera* m_activeCamera = nullptr;
	const Scene* m_activeScene = nullptr;
	Settings m_settings = Settings();

	std::shared_ptr<Walnut::Image> m_Image;
	uint32_t* m_ImageData = nullptr;
	glm::vec4* m_AccumulationBuffer = nullptr;
	std::vector<uint32_t> m_ImageVerticalIter;

	int m_frameindex = 1;
};