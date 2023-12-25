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
bool Intersections::aabbIntersect( boundingBox bbox, glm::vec3 ray_o, glm::vec3 ray_dir )
{
	glm::vec3 dirfrac( 1.0f / ray_dir.x, 1.0f / ray_dir.y, 1.0f / ray_dir.z );

	float t1 = ( bbox.min.x - ray_o.x ) * dirfrac.x;
	float t2 = ( bbox.max.x - ray_o.x ) * dirfrac.x;
	float t3 = ( bbox.min.y - ray_o.y ) * dirfrac.y;
	float t4 = ( bbox.max.y - ray_o.y ) * dirfrac.y;
	float t5 = ( bbox.min.z - ray_o.z ) * dirfrac.z;
	float t6 = ( bbox.max.z - ray_o.z ) * dirfrac.z;

	float tmin = std::max( std::max( std::min( t1, t2 ), std::min( t3, t4 ) ), std::min( t5, t6 ) );
	float tmax = std::min( std::min( std::max( t1, t2 ), std::max( t3, t4 ) ), std::max( t5, t6 ) );

	// If tmax < 0, ray intersects AABB, but entire AABB is behind ray, so reject.
	if ( tmax < 0.0f ) {
		return false;
	}

	// If tmin > tmax, ray does not intersect AABB.
	if ( tmin > tmax ) {
		return false;
	}

	return true;
}

typedef unsigned int udword;

//! Integer representation of a floating-point value.
#define IR(x)	((udword&)x)
#define RAYAABB_EPSILON 0.00001f
bool Intersections::aabbIntersect2(boundingBox bbox, glm::vec3 ray_o, glm::vec3 ray_dir)
{
	bool Inside = true;
	glm::vec3 MaxT{ -1.0f };
	glm::vec3 coord{ 0.0f };
	

	// Find candidate planes.
	for (udword i = 0; i < 3; i++)
	{
		if (ray_o[i] < bbox.min[i])
		{
			coord[i] = bbox.min[i];
			Inside = false;

			// Calculate T distances to candidate planes
			if (IR(ray_dir[i]))	MaxT[i] = (bbox.min[i] - ray_o[i]) / ray_dir[i];
		}
		else if (ray_o[i] > bbox.max[i])
		{
			coord[i] = bbox.max[i];
			Inside = false;

			// Calculate T distances to candidate planes
			if (IR(ray_dir[i]))	MaxT[i] = (bbox.max[i] - ray_o[i]) / ray_dir[i];
		}
	}

	// Ray origin inside bounding box
	if (Inside)
	{
		return true;
	}

	// Get largest of the maxT's for final choice of intersection
	udword WhichPlane = 0;
	if (MaxT[1] > MaxT[WhichPlane])	WhichPlane = 1;
	if (MaxT[2] > MaxT[WhichPlane])	WhichPlane = 2;

	// Check final candidate actually inside box
	if (IR(MaxT[WhichPlane]) & 0x80000000) return false;

	for (udword i = 0; i < 3; i++)
	{
		if (i != WhichPlane)
		{
			coord[i] = ray_o[i] + MaxT[WhichPlane] * ray_dir[i];
#ifdef RAYAABB_EPSILON
			if (coord[i] < bbox.min[i] - RAYAABB_EPSILON || coord[i] > bbox.max[i] + RAYAABB_EPSILON)	return false;
#else
			if (coord[i] < bbox.min[i] || coord[i] > bbox.max[i])	return false;
#endif
		}
	}
	return true;	// ray hits box
}


////////////////////////////////////////////////////
// Fast, minimum storage ray/triangle intersection test.
// Implementation inspired by Tomas Moller: http://www.graphics.cornell.edu/pubs/1997/MT97.pdf
// Additional algorithm details: http://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
////////////////////////////////////////////////////
bool Intersections::triIntersect( glm::vec3 ray_o, glm::vec3 ray_dir, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float &t, float& _u, float& _v)
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