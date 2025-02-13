#pragma once
#include "BVH.h"
#include "Intersections.h"

BVH::BVH(const Scene& scene)
{
	allTriangles = scene.triangles;

	// Create root node
	Node root;
	for (const auto& tri : allTriangles) {
		root.boundingBox.growToInclude(tri);
	}

	allNodes.push_back(root);

	Split(0, 0, static_cast<int>(scene.triangles.size()));

	for (const auto& tri : allTriangles) {
		TriangleOptimized triOpt;
		triOpt.v0 = tri.Vertices[0];
		triOpt.e1 = tri.Vertices[1] - tri.Vertices[0];
		triOpt.e2 = tri.Vertices[2] - tri.Vertices[0]; 
		allTrianglesOptimized.push_back(triOpt);
	}

}

HitInfo BVH::IntersectRay(Ray* ray)
{
	BVHHitInfo hitInfo;
	int nodeStack[32];
	char stackIndex = 0;
	nodeStack[stackIndex++] = 0;

	while (stackIndex > 0) {
		const Node& currentNode = allNodes[nodeStack[--stackIndex]];

		//float currentNodeDist = Intersections::intersectBB(currentNode.boundingBox, ray);
		//if (currentNodeDist > hitInfo.dist)
		//	continue;

		bool isLeaf = (currentNode.triangleCount > 0);
		if (isLeaf) {
			// Check all triangles in leaf
			TriHitInfo triHit;
			for (int i = currentNode.index; i < currentNode.index + currentNode.triangleCount; i++) {
				bool didHit = Intersections::intersectTri2(allTrianglesOptimized[i], ray, triHit);
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

			float dstA = Intersections::intersectBB(allNodes[childIndexA].boundingBox, ray);
			float dstB = Intersections::intersectBB(allNodes[childIndexB].boundingBox, ray);

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
		returnHit.materialIndex = allTriangles[hitInfo.triIndex].MaterialIndex;
		returnHit.position = ray->Origin + ray->Direction * hitInfo.dist;
		returnHit.normal = glm::normalize((1.0f - hitInfo.u - hitInfo.v) * allTriangles[hitInfo.triIndex].Normals[0] + hitInfo.u * allTriangles[hitInfo.triIndex].Normals[1] + hitInfo.v * allTriangles[hitInfo.triIndex].Normals[2]);
	}

	return returnHit;
}


void BVH::Split(int parentIndex, int triIndex, int triNum, int depth)
{
	const int MaxDepth = 32;
	Node* parent = &(allNodes[parentIndex]);

	float parentCost = triNum * parent->boundingBox.getArea();
	SplitInfo bestSplit = ChooseSplitAxis(allNodes[parentIndex].boundingBox, triIndex, triNum);

	if (depth < MaxDepth && bestSplit.cost < parentCost) {
		BoundingBox bbLeft, bbRight;
		int triCountLeft = 0;

		for (int i = triIndex; i < triIndex + triNum; i++) {
			const TriangleOBJ& tri = allTriangles[i];

			if (tri.Center[bestSplit.axis] < bestSplit.plane) {
				bbLeft.growToInclude(tri);

				TriangleOBJ swapTri = allTriangles[triIndex + triCountLeft];
				allTriangles[triIndex + triCountLeft] = tri;
				allTriangles[i] = swapTri;
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

		allNodes.push_back(leftChild);
		int childIndexLeft = static_cast<int>(allNodes.size() - 1);
		allNodes.push_back(rightChild);
		int childIndexRight = static_cast<int>(allNodes.size() - 1);

		allNodes[parentIndex].index = childIndexLeft;

		Split(childIndexLeft, triIndex, triCountLeft, depth + 1);
		Split(childIndexRight, triIndex + triCountLeft, triCountRight, depth + 1);
	}
	// Leaf node
	else {
		allNodes[parentIndex].index = triIndex;
		allNodes[parentIndex].triangleCount = triNum;
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
			//float splitPlane = boundingBox.min[splitAxis] + delta * (boundingBox.max[splitAxis] - boundingBox.min[splitAxis]);
			float splitPlane = boundingBox.center[splitAxis] - boundingBox.extends[splitAxis] + delta * boundingBox.extends[splitAxis] * 2.0f;

			int triCountLeft = 0;
			int triCountRight = 0;

			BoundingBox bbLeft, bbRight;

			for (int i = triIndex; i < triIndex + triNum; i++) {
				const TriangleOBJ& tri = allTriangles[i];
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
