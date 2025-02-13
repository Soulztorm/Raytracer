#pragma once
#include "Scene.h"
#include "Ray.h"
#include "BVHStructs.h"

struct HitInfo {
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 normal = glm::vec3(0.f);
	int materialIndex = -1;
};

struct SplitInfo {
	char axis = 0;
	float plane = 0.0f;
	float cost = FLT_MAX;
};

class BVH {
public:
	BVH(const Scene& scene);
	HitInfo IntersectRay(Ray* ray);

	size_t GetNodeCount() { return m_nodes.size(); }

protected:
	void Split(int parentIndex, int triIndex, int triNum, int depth = 0);
	SplitInfo ChooseSplitAxis(const BoundingBox& boundingBox, int triIndex, int triNum);

	std::vector<TriangleOBJ> m_trianglesOBJ;

	std::vector<Node> m_nodes;
	std::vector<TriangleOptimized> m_trianglesOptimized;

};

