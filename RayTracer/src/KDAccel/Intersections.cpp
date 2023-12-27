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
bool Intersections::aabbIntersect(const boundingBox& bbox, const glm::vec3& ray_o, const glm::vec3& ray_dir_inv)
{
	// Early out, ray origin inside bb
	if (ray_o.x > bbox.min.x && ray_o.x < bbox.max.x &&
		ray_o.y > bbox.min.y && ray_o.y < bbox.max.y &&
		ray_o.z > bbox.min.z && ray_o.z < bbox.max.z)
		return true;

	float t1 = ( bbox.min.x - ray_o.x ) * ray_dir_inv.x;
	float t2 = ( bbox.max.x - ray_o.x ) * ray_dir_inv.x;
	float t3 = ( bbox.min.y - ray_o.y ) * ray_dir_inv.y;
	float t4 = ( bbox.max.y - ray_o.y ) * ray_dir_inv.y;
	float t5 = ( bbox.min.z - ray_o.z ) * ray_dir_inv.z;
	float t6 = ( bbox.max.z - ray_o.z ) * ray_dir_inv.z;

	float tmax = std::min( std::min( std::max( t1, t2 ), std::max( t3, t4 ) ), std::max( t5, t6 ) );

	// If tmax < 0, ray intersects AABB, but entire AABB is behind ray, so reject.
	if ( tmax < 0.0f ) {
		return false;
	}

	float tmin = std::max( std::max( std::min( t1, t2 ), std::min( t3, t4 ) ), std::min( t5, t6 ) );

	// If tmin > tmax, ray does not intersect AABB.
	if ( tmin > tmax ) {
		return false;
	}

	return true;
}


////////////////////////////////////////////////////
// Fast, minimum storage ray/triangle intersection test.
// Implementation inspired by Tomas Moller: http://www.graphics.cornell.edu/pubs/1997/MT97.pdf
// Additional algorithm details: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
////////////////////////////////////////////////////
bool Intersections::triIntersect(const glm::vec3& ray_o, const glm::vec3& ray_dir, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float &t, float& _u, float& _v)
{
	glm::vec3 e1, e2, h, s, q;
	float a, f, u, v;

	e1 = v1 - v0;
	e2 = v2 - v0;

	h = glm::cross( ray_dir, e2 );
	a = glm::dot( e1, h );

	if ( a > -0.00001f && a < 0.00001f ) {
		return false;
	}

	f = 1.0f / a;
	s = ray_o - v0;
	u = f * glm::dot( s, h );

	if ( u < 0.0f || u > 1.0f ) {
		return false;
	}

	q = glm::cross( s, e1 );
	v = f * glm::dot( ray_dir, q );

	if ( v < 0.0f || u + v > 1.0f ) {
		return false;
	}

	// at this stage we can compute t to find out where the intersection point is on the line
	t = f * glm::dot( e2, q );

	if ( t > 0.00001f ) { // ray intersection
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