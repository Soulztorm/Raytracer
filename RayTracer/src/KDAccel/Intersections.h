#pragma once

#ifndef INTERSECTIONS_H
#define INTERSECTIONS_H

#include "KDTreeStructs.h"
#include <glm/glm.hpp>
#include "../Ray.h"


class Intersections
{
public:
	Intersections( void );
	~Intersections( void );

	static bool aabbIntersect( const boundingBox&  bbox, Ray* ray, float& t_near);
	static bool triIntersect(Ray* ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float &t, float& _u, float& _v);

	static glm::vec3 computeTriNormal( const glm::vec3&, const glm::vec3&, const glm::vec3& );
};

#endif