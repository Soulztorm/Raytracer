#ifndef INTERSECTIONS_H
#define INTERSECTIONS_H

#include "KDTreeStructs.h"
#include <glm/glm.hpp>


class Intersections
{
public:
	Intersections( void );
	~Intersections( void );

	static bool aabbIntersect( boundingBox bbox, glm::vec3 ray_o, glm::vec3 ray_dir);
	static bool aabbIntersect2( boundingBox bbox, glm::vec3 ray_o, glm::vec3 ray_dir);
	static bool triIntersect( glm::vec3 ray_o, glm::vec3 ray_dir, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float &t, float& _u, float& _v);

	static glm::vec3 computeTriNormal( const glm::vec3&, const glm::vec3&, const glm::vec3& );
};

#endif