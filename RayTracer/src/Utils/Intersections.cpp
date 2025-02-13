#include "Intersections.h"

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
	glm::vec3 h = glm::cross( ray->Direction, tri.e2 );
	float a = glm::dot(tri.e1, h );

	if ( a > -FLT_EPSILON && a < FLT_EPSILON) {
		return false;
	}

	float f = 1.0f / a;
	glm::vec3 s = ray->Origin - tri.v0;
	float u = f * glm::dot( s, h );

	if ( u < 0.0f || u > 1.0f ) {
		return false;
	}

	glm::vec3 q = glm::cross( s, tri.e1 );
	float v = f * glm::dot( ray->Direction, q );

	if ( v < 0.0f || u + v > 1.0f ) {
		return false;
	}

	// at this stage we can compute t to find out where the intersection point is on the line
	float t = f * glm::dot(tri.e2, q );

	if ( t > FLT_EPSILON) { // ray intersection
		hitInfo.dist = t;
		hitInfo.u = u;
		hitInfo.v = v;
		return true;
	}
	
	return false;
}

// Faster and ignores backfaces
bool Intersections::intersectTri2(const TriangleOptimized& tri, Ray* ray, BVHHitInfo& hitInfo)
{
	glm::vec3 normalVector = glm::cross(tri.e1, tri.e2);

	glm::vec3 ao = ray->Origin - tri.v0;
	glm::vec3 dao = glm::cross(ao, ray->Direction);

	float determinant = -glm::dot(ray->Direction, normalVector);
	// Backface culling
	#if 0
		if (determinant < FLT_EPSILON)
			return false;
	#endif

	float invDet = 1.0f / determinant;

	// Calculate dst to triangle & barycentric coordinates of intersection point
	float dst = glm::dot(ao, normalVector) * invDet;
	if (dst < 0)
		return false;

	float u = glm::dot(tri.e2, dao) * invDet;
	float v = -glm::dot(tri.e1, dao) * invDet;
	float w = 1.0f - u - v;

	if (u >= 0.f && v >= 0.f && w >= 0.f) {
		hitInfo.dist = dst;
		hitInfo.u = u;
		hitInfo.v = v;
		return true;
	}

	return false;
}