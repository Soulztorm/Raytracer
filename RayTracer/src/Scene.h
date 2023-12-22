#pragma once

#include <glm/glm.hpp>
#include <Walnut/Random.h>

#include <vector>

struct Sphere {
	glm::vec3 Position{ 0.0f };
	float Radius = 1.0f;

	glm::vec3 Color{ 1.0f };
};

struct Scene {
	std::vector<Sphere> spheres;
};