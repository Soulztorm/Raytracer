#pragma once

#include <glm/glm.hpp>
#include <Walnut/Random.h>

#include "HDRI.h"

#include <vector>

struct TriangleOBJ
{
	std::vector<glm::vec3> Vertices;
	std::vector<glm::vec3> Normals;
	std::vector<glm::vec2> TCoords;
	glm::vec3 Center;
	int MaterialIndex = -1;
};

struct TriangleOptimized
{
	glm::vec3 v0;
	glm::vec3 e1;
	glm::vec3 e2;
	glm::vec3 normal0;
	glm::vec3 normal1;
	glm::vec3 normal2;
	glm::vec2 uv0;
	glm::vec2 uv1;
	glm::vec2 uv2;

	int materialIndex = -1;
};

struct Texture {
	int width = -1;
	int height = -1;
	std::vector<glm::vec4> data;
};

struct Material {
	std::string Name = "Default";

	glm::vec3 Albedo{ 1.0f };
	glm::vec3 Emission{ 0.0f };
	float Metallic = 0.0f;
	float Roughness = 1.0f;
	float Transparency = 0.0f;
	float IOR = 1.0f;

	// Textures
	Texture TexDiffuse;
	Texture TexSpecular;
};

struct Scene {
	std::vector<TriangleOBJ> triangles;
	std::vector<Material> materials;
	HDRI hdri;
};