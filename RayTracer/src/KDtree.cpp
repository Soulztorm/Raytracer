#pragma once

#include "KDtree.h"

#define COMPILE 0
#if COMPILE

bool KDtree::Initialize(const Scene& scene, int maxTrisPerNode)
{
	// Create root node
	m_rootNode.BoundingBox = FindAABBForTriangles(scene.triangles);
	for (int i = 0; i < scene.triangles.size(); i++)
	{
		m_rootNode.Triangles.push_back(scene.triangles.at(i));
	}

	if (scene.triangles.size() > maxTrisPerNode)
		m_rootNode.Divide(maxTrisPerNode, 0);

	return true;
}

AABB KDtree::FindAABBForTriangles(const std::vector<Triangle>& triangles)
{
	AABB aabb;
	for each (const Triangle& tri in triangles)
	{
		for each (const vec3 & vertex in tri.Vertices)
		{
			aabb.Min = min(aabb.Min, vertex);
			aabb.Max = max(aabb.Max, vertex);
		}
	}

	return aabb;
}

uint32_t GetNumberOfVerticesInBB(const Triangle& tri, const AABB& bb) {
	uint32_t numInside = 0;

	for each (const vec3& vertPos in tri.Vertices)
	{
		if (vertPos.x >= bb.Min.x && vertPos.x < bb.Max.x &&
			vertPos.y >= bb.Min.y && vertPos.y < bb.Max.y &&
			vertPos.z >= bb.Min.z && vertPos.z < bb.Max.z)
			numInside++;
	}
	return numInside;
}

bool KDnode::Divide(int maxTrisPerNode, int depth)
{
	if (depth > 50)
		return false;

	int axis = depth % 3;
	
	// Do not split with less than 2 tris, or less than the maxTrisPerNode
	if (Triangles.size() < 2 || Triangles.size() < maxTrisPerNode)
		return false;


	// Sort centroids by current axis
	std::sort(Triangles.begin(), Triangles.end(), [axis](const Triangle& t1, const Triangle& t2) { return t1.Centroid[axis] < t2.Centroid[axis]; });

	// Find the median splitting plane
	uint32_t medianIndex = Triangles.size() / 2;
	float medianPlaneCoord = 0.5f * (Triangles[medianIndex].Centroid[axis] + Triangles[medianIndex + 1].Centroid[axis]);

	// Create 2 new bounding boxes for the childs
	AABB aabb_left = BoundingBox;
	AABB aabb_right = BoundingBox;
	aabb_left.Max[axis] = medianPlaneCoord;
	aabb_right.Min[axis] = medianPlaneCoord;

	// Median plane outside the Bounding box of the current node
	if (medianPlaneCoord > BoundingBox.Max[axis] || medianPlaneCoord < BoundingBox.Min[axis])
		return false;

	std::vector<Triangle> leftChildTris;
	std::vector<Triangle> rightChildTris;

	// Put the triangles into each child
	for each (const Triangle & tri in Triangles)
	{
		uint32_t numVertsInLeftChild = GetNumberOfVerticesInBB(tri, aabb_left);
		uint32_t numVertsInRightChild = GetNumberOfVerticesInBB(tri, aabb_right);

		if (numVertsInLeftChild > 0)
			leftChildTris.push_back(tri);

		if (numVertsInRightChild > 0)
			rightChildTris.push_back(tri);
	}

	// We have left children, add the node, divide if too much tris inside
	if (!leftChildTris.empty()) {
		LeftChild = new KDnode();
		LeftChild->BoundingBox = aabb_left;
		LeftChild->Triangles = leftChildTris;

		LeftChild->Divide(maxTrisPerNode, depth + 1);
	}

	// We have right children, add the node, divide if too much tris inside
	if (!rightChildTris.empty()) {
		RightChild = new KDnode();
		RightChild->BoundingBox = aabb_right;
		RightChild->Triangles = rightChildTris;

		RightChild->Divide(maxTrisPerNode, depth + 1);
	}


	return false;
}

#endif