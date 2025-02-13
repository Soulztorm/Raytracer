#pragma once

#include <glm/glm.hpp>

#include "BVHStructs.h"
#include "Ray.h"

struct BVHHitInfo {
	float dist = FLT_MAX - 1.0f;
	float u = 0.0f;
	float v = 0.0f;
	int triIndex = -1;
};

struct TriHitInfo {
	float dist = FLT_MAX;
	float u = 0.0f;
	float v = 0.0f;
};

class Intersections
{
public:
	static float intersectBB(const BoundingBox& bbox, Ray* ray);
	static bool intersectTri(const TriangleOptimized& tri, Ray* ray, TriHitInfo& hitInfo);
	static bool intersectTri2(const TriangleOptimized& tri, Ray* ray, TriHitInfo& hitInfo);
};