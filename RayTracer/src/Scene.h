#pragma once

#include <glm/glm.hpp>
#include <Walnut/Random.h>

#include "KDAccel/KDTreeCPU.h"

#include <vector>

struct Sphere {
	glm::vec3 Position{ 0.0f };
	float Radius = 1.0f;

	uint32_t MaterialIndex = -1;
};

struct Triangle 
{
	std::vector<glm::vec3> Vertices;
	std::vector<glm::vec3> Normals;

	uint32_t MaterialIndex = -1;
	glm::vec3 Centroid;
};

struct Material {
	std::string Name;

	glm::vec3 Albedo{ 1.0f };
	glm::vec3 Emission{ 0.0f };
	float Roughness = 1.0f;
	float Transparency = 0.0f;
	float IOR = 1.0f;
	//float Metallic = 0.0f;
};

struct Scene {
	std::vector<Sphere> spheres;
	std::vector<Material> materials;

	std::vector<Triangle> triangles;

	std::shared_ptr<KDTreeCPU> kd_tree = nullptr;
};