#pragma once
#include "BVH.h"

#define MAX_DEPTH 32

BVH::BVH(const Scene& scene)
{
	m_trianglesOBJ = scene.triangles;

	// Create root node
	Node root;
	for (const auto& tri : m_trianglesOBJ) {
		root.boundingBox.growToInclude(tri);
	}

	m_nodes.push_back(root);

	Split(0, 0, static_cast<int>(scene.triangles.size()));

	for (const auto& tri : m_trianglesOBJ) {
		TriangleOptimized triOpt;
		triOpt.v0 = tri.Vertices[0];
		triOpt.e1 = tri.Vertices[1] - tri.Vertices[0];
		triOpt.e2 = tri.Vertices[2] - tri.Vertices[0]; 
		
		triOpt.normal0 = tri.Normals[0];
		triOpt.normal1 = tri.Normals[1];
		triOpt.normal2 = tri.Normals[2];

		triOpt.materialIndex = tri.MaterialIndex;

		m_trianglesOptimized.push_back(triOpt);
	}
	m_trianglesOBJ.clear();
}

HitInfo BVH::IntersectRay(Ray* ray)
{
	BVHHitInfo hitInfo;
	int nodeStack[MAX_DEPTH];
	char stackIndex = 0;
	nodeStack[stackIndex++] = 0;

	while (stackIndex > 0) {
		const Node& currentNode = m_nodes[nodeStack[--stackIndex]];

		bool isLeaf = (currentNode.triangleCount > 0);
		if (isLeaf) {
			// Check all triangles in leaf
			BVHHitInfo triHit;
			for (int i = currentNode.index; i < currentNode.index + currentNode.triangleCount; i++) {
				bool didHit = Intersections::intersectTri(m_trianglesOptimized[i], ray, triHit);
				if (didHit && triHit.dist < hitInfo.dist) {
					hitInfo.triIndex = i;
					hitInfo.dist = triHit.dist;
					hitInfo.u = triHit.u;
					hitInfo.v = triHit.v;
				}
			}
		}
		else {
			int childIndexA = currentNode.index;
			int childIndexB = currentNode.index + 1;

			float dstA = Intersections::intersectBB(m_nodes[childIndexA].boundingBox, ray);
			float dstB = Intersections::intersectBB(m_nodes[childIndexB].boundingBox, ray);

			// We want to look at closest child node first, so push it last
			bool isNearestA = dstA <= dstB;
			float dstNear = isNearestA ? dstA : dstB;
			float dstFar = isNearestA ? dstB : dstA;
			int childIndexNear = isNearestA ? childIndexA : childIndexB;
			int childIndexFar = isNearestA ? childIndexB : childIndexA;

			if (dstFar < hitInfo.dist) nodeStack[stackIndex++] = childIndexFar;
			if (dstNear < hitInfo.dist) nodeStack[stackIndex++] = childIndexNear;
		}
	}
	

	HitInfo returnHit;

	// If we hit something, work out the position, normal and material
	if (hitInfo.triIndex >= 0) {
		returnHit.position = ray->Origin + ray->Direction * hitInfo.dist;
		returnHit.normal = (1.0f - hitInfo.u - hitInfo.v) * m_trianglesOptimized[hitInfo.triIndex].normal0 + hitInfo.u * m_trianglesOptimized[hitInfo.triIndex].normal1 + hitInfo.v * m_trianglesOptimized[hitInfo.triIndex].normal2;
		returnHit.materialIndex = m_trianglesOptimized[hitInfo.triIndex].materialIndex;
	}

	return returnHit;
}


void BVH::Split(int parentIndex, int triIndex, int triNum, int depth)
{
	Node* parent = &(m_nodes[parentIndex]);

	float parentCost = triNum * parent->boundingBox.getArea();
	// Surface Area Heuristic split
	SplitInfo bestSplit = ChooseSplitAxis(m_nodes[parentIndex].boundingBox, triIndex, triNum);

	// Not to deep, and the split would improve cost
	if (depth < MAX_DEPTH && bestSplit.cost < parentCost) {
		BoundingBox bbLeft, bbRight;
		int triCountLeft = 0;

		for (int i = triIndex; i < triIndex + triNum; i++) {
			const TriangleOBJ& tri = m_trianglesOBJ[i];

			if (tri.Center[bestSplit.axis] < bestSplit.plane) {
				bbLeft.growToInclude(tri);

				TriangleOBJ swapTri = m_trianglesOBJ[triIndex + triCountLeft];
				m_trianglesOBJ[triIndex + triCountLeft] = tri;
				m_trianglesOBJ[i] = swapTri;
				triCountLeft++;
			}
			else {
				bbRight.growToInclude(tri);
			}
		}

		int triCountRight = triNum - triCountLeft;
		int triStartRight = triIndex + triCountLeft;

		Node leftChild = { bbLeft, triIndex, 0 };
		Node rightChild = { bbRight, triStartRight, 0 };

		m_nodes.push_back(leftChild);
		m_nodes.push_back(rightChild);
		int childIndexLeft = static_cast<int>(m_nodes.size() - 2);
		int childIndexRight = static_cast<int>(m_nodes.size() - 1);

		m_nodes[parentIndex].index = childIndexLeft;

		Split(childIndexLeft, triIndex, triCountLeft, depth + 1);
		Split(childIndexRight, triIndex + triCountLeft, triCountRight, depth + 1);
	}
	// Leaf node
	else {
		m_nodes[parentIndex].index = triIndex;
		m_nodes[parentIndex].triangleCount = triNum;
	}
}

SplitInfo BVH::ChooseSplitAxis(const BoundingBox& boundingBox, int triIndex, int triNum)
{
	SplitInfo result;

	// Less than 2 triangles, no need to split
	if (triNum < 2)
		return result;


	for (char splitAxis = 0; splitAxis < 3; splitAxis++) {
		for (float delta = 0.0f; delta < 1.0f; delta += 0.1f) {
			float splitPlane = boundingBox.center[splitAxis] - boundingBox.extends[splitAxis] + delta * boundingBox.extends[splitAxis] * 2.0f;

			int triCountLeft = 0;
			int triCountRight = 0;

			BoundingBox bbLeft, bbRight;

			for (int i = triIndex; i < triIndex + triNum; i++) {
				const TriangleOBJ& tri = m_trianglesOBJ[i];
				if (tri.Center[splitAxis] < splitPlane) {
					bbLeft.growToInclude(tri);
					triCountLeft++;
				}
				else {
					bbRight.growToInclude(tri);
					triCountRight++;
				}
			}

			// If this split causes any child to have no triangles, it is effectively the same as the parent, skip
			if (triCountLeft == 0 || triCountRight == 0)
				continue;

			float cost = triCountLeft * bbLeft.getArea() + triCountRight * bbRight.getArea();
			if (cost < result.cost) {
				result.cost = cost;
				result.axis = splitAxis;
				result.plane = splitPlane;
			}
		}	
	}

	return result;
}






////////////////////////////////////////////////////
// Fast ray/AABB intersection test.
// Implementation inspired by zacharmarz.
// https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms
////////////////////////////////////////////////////
float Intersections::intersectBB(const BoundingBox& bbox, Ray* ray)
{
	glm::vec3 l1 = (bbox.center - ray->Origin) * ray->DirectionInverse;
	glm::vec3 l2 = bbox.extends * ray->DirectionInverse;

	glm::vec3 tMin = l1 - l2;
	glm::vec3 tMax = l1 + l2;

	glm::vec3 t2 = glm::max(tMin, tMax);
	float tFar = std::min(std::min(t2.x, t2.y), t2.z);

	if (tFar < 0.0f)
		return FLT_MAX;

	glm::vec3 t1 = glm::min(tMin, tMax);
	float tNear = std::max(std::max(t1.x, t1.y), t1.z);

	if (tNear > tFar)
		return FLT_MAX;

	return tNear;
}

////////////////////////////////////////////////////
// Fast, minimum storage ray/triangle intersection test.
// Implementation inspired by Tomas Moller: http://www.graphics.cornell.edu/pubs/1997/MT97.pdf
// Additional algorithm details: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
////////////////////////////////////////////////////
bool Intersections::intersectTri(const TriangleOptimized& tri, Ray* ray, BVHHitInfo& hitInfo)
{
	glm::vec3 h = glm::cross(ray->Direction, tri.e2);
	float a = glm::dot(tri.e1, h);

	if (a > -FLT_EPSILON && a < FLT_EPSILON) {
		return false;
	}

	float f = 1.0f / a;
	glm::vec3 s = ray->Origin - tri.v0;
	float u = f * glm::dot(s, h);

	if (u < 0.0f || u > 1.0f) {
		return false;
	}

	glm::vec3 q = glm::cross(s, tri.e1);
	float v = f * glm::dot(ray->Direction, q);

	if (v < 0.0f || u + v > 1.0f) {
		return false;
	}

	// at this stage we can compute t to find out where the intersection point is on the line
	float t = f * glm::dot(tri.e2, q);

	if (t > FLT_EPSILON) { // ray intersection
		hitInfo.dist = t;
		hitInfo.u = u;
		hitInfo.v = v;
		return true;
	}

	return false;
}