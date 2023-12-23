#pragma once

#include <glm/glm.hpp>
#include <Walnut/Random.h>

#include <vector>

struct Sphere {
	glm::vec3 Position{ 0.0f };
	float Radius = 1.0f;

	uint32_t MaterialIndex = -1;
};

struct Material {
	glm::vec3 Albedo{ 1.0f };
	float Roughness = 1.0f;
	float Metallic = 0.0f;
};

struct Scene {
	std::vector<Sphere> spheres;
	std::vector<Material> materials;

	glm::vec3 lightPosition{ 0.0f, 9.5f, 0.0f };
	float lightPower = 0.0f;
};