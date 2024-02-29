#include "Intersections.h"
#include <algorithm>


////////////////////////////////////////////////////
// Constructor/destructor.
////////////////////////////////////////////////////

Intersections::Intersections()
{
}

Intersections::~Intersections()
{
}


////////////////////////////////////////////////////
// Fast ray/AABB intersection test.
// Implementation inspired by zacharmarz.
// https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms
////////////////////////////////////////////////////
bool Intersections::aabbIntersect(const boundingBox& bbox, const glm::vec3& ray_o, const glm::vec3& ray_dir_inv, float& t_near, float& t_far)
{
	glm::vec3 tMin = (bbox.center - bbox.extends - ray_o) * ray_dir_inv;
	glm::vec3 tMax = (bbox.center + bbox.extends - ray_o) * ray_dir_inv;
	glm::vec3 t2 = glm::max(tMin, tMax);
	float tFar = std::min(std::min(t2.x, t2.y), t2.z);

	if (tFar < 0.0f)
		return false;

	glm::vec3 t1 = glm::min(tMin, tMax);
	float tNear = std::max(std::max(t1.x, t1.y), t1.z);

	if (tNear > tFar)
		return false;

	t_near = tNear;
	t_far = tFar;

	return true;
}



////////////////////////////////////////////////////
// Fast, minimum storage ray/triangle intersection test.
// Implementation inspired by Tomas Moller: http://www.graphics.cornell.edu/pubs/1997/MT97.pdf
// Additional algorithm details: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
////////////////////////////////////////////////////
bool Intersections::triIntersect(const glm::vec3& ray_o, const glm::vec3& ray_dir, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float &t, float& _u, float& _v)
{
	glm::vec3 e1 = v1 - v0;
	glm::vec3 e2 = v2 - v0;

	glm::vec3 h = glm::cross( ray_dir, e2 );
	float a = glm::dot( e1, h );

	if ( a > -0.00001f && a < 0.00001f) {
		return false;
	}

	float f = 1.0f / a;
	glm::vec3 s = ray_o - v0;
	float u = f * glm::dot( s, h );

	if ( u < 0.0f || u > 1.0f ) {
		return false;
	}

	glm::vec3 q = glm::cross( s, e1 );
	float v = f * glm::dot( ray_dir, q );

	if ( v < 0.0f || u + v > 1.0f ) {
		return false;
	}

	// at this stage we can compute t to find out where the intersection point is on the line
	t = f * glm::dot( e2, q );

	if ( t > 0.00001f) { // ray intersection
		_u = u;
		_v = v;
		return true;
	}
	else { // this means that there is a line intersection but not a ray intersection
		return false;
	}
}


////////////////////////////////////////////////////
// computeTriNormal().
////////////////////////////////////////////////////
glm::vec3 Intersections::computeTriNormal( const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3 )
{
	glm::vec3 u = p2 - p1;
	glm::vec3 v = p3 - p1;

	float nx = u.y * v.z - u.z * v.y;
	float ny = u.z * v.x - u.x * v.z;
	float nz = u.x * v.y - u.y * v.x;

	return glm::normalize( glm::vec3( nx, ny, nz ) );
}