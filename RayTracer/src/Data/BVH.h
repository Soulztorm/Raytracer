#pragma once
#include "Scene.h"
#include "Ray.h"

class BoundingBox
{
public:
	glm::vec3 center = glm::vec3(0.0f);
	glm::vec3 extends = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	void growToInclude(const TriangleOBJ& tri) {
		glm::vec3 min = this->center - this->extends;
		glm::vec3 max = this->center + this->extends;

		for (int i = 0; i < 3; ++i) {
			min = glm::min(min, tri.Vertices[i]);
			max = glm::max(max, tri.Vertices[i]);
		}

		this->center = 0.5f * (min + max);
		this->extends = 0.5f * (max - min);
	}

	const float getArea() const {
		float xy = 2.0f * extends[0] * extends[1];
		float yz = 2.0f * extends[1] * extends[2];
		float zx = 2.0f * extends[2] * extends[0];
		return xy + yz + zx;
	}
};

struct SplitInfo {
	char axis = 0;
	float plane = 0.0f;
	float cost = FLT_MAX;
};

struct Node {
	BoundingBox boundingBox;
	int index = -1;
	int triangleCount = -1;
};

struct HitInfo {
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 normal = glm::vec3(0.f);
	int materialIndex = -1;
};

class BVH {
public:
	BVH(const Scene& scene);
	HitInfo IntersectRay(Ray* ray);

	size_t GetNodeCount() { return m_nodes.size(); }

	std::vector<Node>* GetNodes() { return &m_nodes; }
	std::vector<TriangleOptimized>* GetTrisOpt() { return &m_trianglesOptimized; }

protected:
	void Split(int parentIndex, int triIndex, int triNum, int depth = 0);
	SplitInfo ChooseSplitAxis(const BoundingBox& boundingBox, int triIndex, int triNum);

	std::vector<TriangleOBJ> m_trianglesOBJ;

	std::vector<Node> m_nodes;
	std::vector<TriangleOptimized> m_trianglesOptimized;
};





// Intersection stuff
struct BVHHitInfo {
	float dist = FLT_MAX - 1.0f;
	float u = 0.0f;
	float v = 0.0f;
	int triIndex = -1;
};

class Intersections
{
public:
	static float intersectBB(const BoundingBox& bbox, Ray* ray);
	static bool intersectTri(const TriangleOptimized& tri, Ray* ray, BVHHitInfo& hitInfo);
};
