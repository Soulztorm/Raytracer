#pragma once

#include <glm/glm.hpp>
#include <Walnut/Random.h>

#include <vector>

struct Sphere {
	glm::vec3 Position{ 0.0f };
	float Radius = 1.0f;

	uint32_t MaterialIndex = -1;
};

struct Triangle 
{
	std::vector<glm::vec3> Vertices;
	glm::vec3 Normal { 0.0f };

	uint32_t MaterialIndex = -1;
};

struct Material {
	glm::vec3 Albedo{ 1.0f };
	float Roughness = 1.0f;
	float Metallic = 0.0f;
	float Emission = 0.0f;
};

struct Scene {
	std::vector<Sphere> spheres;
	std::vector<Material> materials;

	std::vector<Triangle> triangles;
};