#pragma once

#include <glm/glm.hpp>
#include <Walnut/Random.h>

#include <vector>

struct TriangleOBJ
{
	std::vector<glm::vec3> Vertices;
	std::vector<glm::vec3> Normals;
	glm::vec3 Center;
	uint32_t MaterialIndex = -1;
};

struct TriNormalsMats {
	std::vector<glm::vec3> Normals;
	uint32_t MaterialIndex = -1;
};

struct TriangleOptimized
{
	glm::vec3 v0;
	glm::vec3 e1;
	glm::vec3 e2;

	glm::vec3 normal0;
	glm::vec3 normal1;
	glm::vec3 normal2;

	uint32_t materialIndex;
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
	std::vector<Material> materials;
	std::vector<TriangleOBJ> triangles;
};