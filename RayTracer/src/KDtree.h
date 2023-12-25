#pragma once

#include "Scene.h"

#include <glm/glm.hpp>

using namespace glm;

#define COMPILE 0
#if COMPILE

struct AABB {
	vec3 Min{ FLT_MAX, FLT_MAX, FLT_MAX };
	vec3 Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
};

class KDnode {

public:
	KDnode() = default;
	~KDnode() { 
		delete LeftChild;
		delete RightChild;
	}

	bool Divide(int maxTrisPerNode, int depth);

	AABB BoundingBox;

	std::vector<Triangle> Triangles;

	KDnode* LeftChild = nullptr;
	KDnode* RightChild = nullptr;
};

class KDtree {
public:
	KDtree() = default;

	bool Initialize(const Scene& scene, int maxTrisPerNode = 10);

	AABB FindAABBForTriangles(const std::vector<Triangle>& triangles);

private:
	bool m_initialized = false;

	KDnode m_rootNode;
};

#endif