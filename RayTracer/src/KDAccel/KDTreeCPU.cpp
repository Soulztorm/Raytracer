#include "KDTreeCPU.h"
#include <algorithm>
#include "Intersections.h"
#include <iostream>


////////////////////////////////////////////////////
// Constructor/destructor.
////////////////////////////////////////////////////

KDTreeCPU::KDTreeCPU( int num_tris, glm::vec3 *tris, int num_verts, glm::vec3 *verts )
{
	// Set class-level variables.
	num_levels = 0;
	num_leaves = 0;
	num_nodes = 0;
	this->num_verts = num_verts;
	this->num_tris = num_tris;

	this->verts = new glm::vec3[num_verts];
	for ( int i = 0; i < num_verts; ++i ) {
		this->verts[i] = verts[i];
	}

	this->tris = new glm::vec3[num_tris];
	for ( int i = 0; i < num_tris; ++i ) {
		this->tris[i] = tris[i];
	}

	// Create list of triangle indices for first level of kd-tree.
	int *tri_indices = new int[num_tris];
	for ( int i = 0; i < num_tris; ++i ) {
		tri_indices[i] = i;
	}

	// Compute bounding box for all triangles.
	boundingBox bbox = computeTightFittingBoundingBox( num_verts, verts );

	// Build kd-tree and set root node.
	root = constructTreeMedianSpaceSplit( num_tris, tri_indices, bbox, 1 );

    // build rope structure
    //KDTreeNode* ropes[6] = { NULL };
    //buildRopeStructure( root, ropes, true );
}

KDTreeCPU::~KDTreeCPU()
{
	if ( num_verts > 0 ) {
		delete[] verts;
	}

	if ( num_tris > 0 ) {
		delete[] tris;
	}

	delete root;
}



////////////////////////////////////////////////////
// Getters.
////////////////////////////////////////////////////

KDTreeNode* KDTreeCPU::getRootNode() const
{
	return root;
}

int KDTreeCPU::getNumLevels() const
{
	return num_levels;
}

int KDTreeCPU::getNumLeaves( void ) const
{
	return num_leaves;
}

int KDTreeCPU::getNumNodes( void ) const
{
	return num_nodes;
}

SplitAxis KDTreeCPU::getLongestBoundingBoxSide( glm::vec3 min, glm::vec3 max )
{
	// max > min is guaranteed.
	float xlength = max.x - min.x;
	float ylength = max.y - min.y;
	float zlength = max.z - min.z;
	return ( xlength > ylength && xlength > zlength ) ? X_AXIS : ( ylength > zlength ? Y_AXIS : Z_AXIS );
}

float KDTreeCPU::getMinTriValue( int tri_index, SplitAxis axis )
{
	glm::vec3 tri = tris[tri_index];
	glm::vec3 v0 = verts[( int )tri[0]];
	glm::vec3 v1 = verts[( int )tri[1]];
	glm::vec3 v2 = verts[( int )tri[2]];

	if ( axis == X_AXIS ) {
		return ( v0.x < v1.x && v0.x < v2.x ) ? v0.x : ( v1.x < v2.x ? v1.x : v2.x );
	}
	else if ( axis == Y_AXIS ) {
		return ( v0.y < v1.y && v0.y < v2.y ) ? v0.y : ( v1.y < v2.y ? v1.y : v2.y );
	}
	else {
		return ( v0.z < v1.z && v0.z < v2.z ) ? v0.z : ( v1.z < v2.z ? v1.z : v2.z );
	}
}

float KDTreeCPU::getMaxTriValue( int tri_index, SplitAxis axis )
{
	glm::vec3 tri = tris[tri_index];
	glm::vec3 v0 = verts[( int )tri[0]];
	glm::vec3 v1 = verts[( int )tri[1]];
	glm::vec3 v2 = verts[( int )tri[2]];

	if ( axis == X_AXIS ) {
		return ( v0.x > v1.x && v0.x > v2.x ) ? v0.x : ( v1.x > v2.x ? v1.x : v2.x );
	}
	else if ( axis == Y_AXIS ) {
		return ( v0.y > v1.y && v0.y > v2.y ) ? v0.y : ( v1.y > v2.y ? v1.y : v2.y );
	}
	else {
		return ( v0.z > v1.z && v0.z > v2.z ) ? v0.z : ( v1.z > v2.z ? v1.z : v2.z );
	}
}

int KDTreeCPU::getMeshNumVerts( void ) const
{
	return num_verts;
}

int KDTreeCPU::getMeshNumTris( void ) const
{
	return num_tris;
}

glm::vec3* KDTreeCPU::getMeshVerts( void ) const
{
	return verts;
}

glm::vec3* KDTreeCPU::getMeshTris( void ) const
{
	return tris;
}


////////////////////////////////////////////////////
// Methods to compute tight fitting bounding boxes around triangles.
////////////////////////////////////////////////////

boundingBox KDTreeCPU::computeTightFittingBoundingBox( int num_verts, glm::vec3 *verts )
{
	// Compute bounding box for input mesh.
	glm::vec3 max = glm::vec3( -INFINITYY, -INFINITYY, -INFINITYY );
	glm::vec3 min = glm::vec3( INFINITYY, INFINITYY, INFINITYY );

	for ( int i = 0; i < num_verts; ++i ) {
		if ( verts[i].x < min.x ) {
			min.x = verts[i].x;
		}
		if ( verts[i].y < min.y ) {
			min.y = verts[i].y;
		}
		if ( verts[i].z < min.z ) {
			min.z = verts[i].z;
		}
		if ( verts[i].x > max.x ) {
			max.x = verts[i].x;
		}
		if ( verts[i].y > max.y ) {
			max.y = verts[i].y;
		}
		if ( verts[i].z > max.z ) {
			max.z = verts[i].z;
		}
	}

	boundingBox bbox;
	bbox.min = min;
	bbox.max = max;

	return bbox;
}

boundingBox KDTreeCPU::computeTightFittingBoundingBox( int num_tris, int *tri_indices )
{
	int num_verts = num_tris * 3;
	glm::vec3 *verts = new glm::vec3[num_verts];

	int verts_index;
	for ( int i = 0; i < num_tris; ++i ) {
		glm::vec3 tri = tris[i];
		verts_index = i * 3;
		verts[verts_index + 0] = this->verts[( int )tri[0]];
		verts[verts_index + 1] = this->verts[( int )tri[1]];
		verts[verts_index + 2] = this->verts[( int )tri[2]];
	}

	boundingBox bbox = computeTightFittingBoundingBox( num_verts, verts );
	delete[] verts;
	return bbox;
}


////////////////////////////////////////////////////
// constructTreeMedianSpaceSplit().
////////////////////////////////////////////////////
KDTreeNode* KDTreeCPU::constructTreeMedianSpaceSplit( int num_tris, int *tri_indices, boundingBox bounds, int curr_depth )
{
	// Create new node.
	KDTreeNode *node = new KDTreeNode();
	node->num_tris = num_tris;
	node->tri_indices = tri_indices;

	// Override passed-in bounding box and create "tightest-fitting" bounding box around passed-in list of triangles.
	if ( USE_TIGHT_FITTING_BOUNDING_BOXES ) {
		node->bbox = computeTightFittingBoundingBox( num_tris, tri_indices );
	}
	else {
		node->bbox = bounds;
	}

	// Base case--Number of triangles in node is small enough.
	if ( num_tris <= NUM_TRIS_PER_NODE  || curr_depth >= MAX_DEPTH) {
		node->is_leaf_node = true;

		// Update number of tree levels.
		if ( curr_depth > num_levels ) {
			num_levels = curr_depth;
		}

		// Set node ID.
		node->id = num_nodes;
		++num_nodes;

		// Return leaf node.
		++num_leaves;
		return node;
	}

	// Get longest side of bounding box.
	SplitAxis longest_side = getLongestBoundingBoxSide( bounds.min, bounds.max );
	node->split_plane_axis = longest_side;

	// Compute median value for longest side as well as "loose-fitting" bounding boxes.
	float median_val = 0.0;
	boundingBox left_bbox = node->bbox;
	boundingBox right_bbox = node->bbox;
	if ( longest_side == X_AXIS ) {
		median_val = bounds.min.x + ( ( bounds.max.x - bounds.min.x ) / 2.0f );
		left_bbox.max.x = median_val;
		right_bbox.min.x = median_val;
	}
	else if ( longest_side == Y_AXIS ) {
		median_val = bounds.min.y + ( ( bounds.max.y - bounds.min.y ) / 2.0f );
		left_bbox.max.y = median_val;
		right_bbox.min.y = median_val;
	}
	else {
		median_val = bounds.min.z + ( ( bounds.max.z - bounds.min.z ) / 2.0f );
		left_bbox.max.z = median_val;
		right_bbox.min.z = median_val;
	}

	node->split_plane_value = median_val;

	// Allocate and initialize memory for temporary buffers to hold triangle indices for left and right subtrees.
	int *temp_left_tri_indices = new int[num_tris];
	int *temp_right_tri_indices = new int[num_tris];

	// Populate temporary buffers.
	int left_tri_count = 0, right_tri_count = 0;
	float min_tri_val, max_tri_val;
	for ( int i = 0; i < num_tris; ++i ) {
		// Get min and max triangle values along desired axis.
		if ( longest_side == X_AXIS ) {
			min_tri_val = getMinTriValue( tri_indices[i], X_AXIS );
			max_tri_val = getMaxTriValue( tri_indices[i], X_AXIS );
		}
		else if ( longest_side == Y_AXIS ) {
			min_tri_val = getMinTriValue( tri_indices[i], Y_AXIS );
			max_tri_val = getMaxTriValue( tri_indices[i], Y_AXIS );
		}
		else {
			min_tri_val = getMinTriValue( tri_indices[i], Z_AXIS );
			max_tri_val = getMaxTriValue( tri_indices[i], Z_AXIS );
		}

		// Update temp_left_tri_indices.
		if ( min_tri_val < median_val ) {
			temp_left_tri_indices[i] = tri_indices[i];
			++left_tri_count;
		}
		else {
			temp_left_tri_indices[i] = -1;
		}

		// Update temp_right_tri_indices.
		if ( max_tri_val >= median_val ) {
			temp_right_tri_indices[i] = tri_indices[i];
			++right_tri_count;
		}
		else {
			temp_right_tri_indices[i] = -1;
		}
	}

	// Allocate memory for lists of triangle indices for left and right subtrees.
	int *left_tri_indices = new int[left_tri_count];
	int *right_tri_indices = new int[right_tri_count];

	// Populate lists of triangle indices.
	int left_index = 0, right_index = 0;
	for ( int i = 0; i < num_tris; ++i ) {
		if ( temp_left_tri_indices[i] != -1 ) {
			left_tri_indices[left_index] = temp_left_tri_indices[i];
			++left_index;
		}
		if ( temp_right_tri_indices[i] != -1 ) {
			right_tri_indices[right_index] = temp_right_tri_indices[i];
			++right_index;
		}
	}

	// Free temporary triangle indices buffers.
	delete[] temp_left_tri_indices;
	delete[] temp_right_tri_indices;

	// Recurse.
	node->left = constructTreeMedianSpaceSplit( left_tri_count, left_tri_indices, left_bbox, curr_depth + 1 );
	node->right = constructTreeMedianSpaceSplit( right_tri_count, right_tri_indices, right_bbox, curr_depth + 1 );

	// Set node ID.
	node->id = num_nodes;
	++num_nodes;

	return node;
}


////////////////////////////////////////////////////
// Recursive (needs a stack) kd-tree traversal method to test for intersections with passed-in ray.
////////////////////////////////////////////////////

// Public-facing wrapper method.
bool KDTreeCPU::intersect( const glm::vec3 &ray_o, const glm::vec3 &ray_dir, float &t, uint32_t& tri_index, float& u, float& v) const
{
	t = INFINITYY;
	return intersect(root, ray_o, ray_dir, t, tri_index, u, v);
}

// Private recursive call.
bool KDTreeCPU::intersect( KDTreeNode *curr_node, const glm::vec3 &ray_o, const glm::vec3 &ray_dir, float &t, uint32_t& tri_index, float& u, float& v) const
{
	// Perform ray/AABB intersection test.
	bool intersects_aabb = Intersections::aabbIntersect( curr_node->bbox, ray_o, ray_dir );

	if ( intersects_aabb ) {
		// If current node is a leaf node.
		if ( !curr_node->left && !curr_node->right ) {
			// Check triangles for intersections.
			bool intersection_detected = false;
			for ( int i = 0; i < curr_node->num_tris; ++i ) {
				int triIndex = curr_node->tri_indices[i];
				glm::vec3 tri = tris[triIndex];
				glm::vec3 v0 = verts[( int )tri[0]];
				glm::vec3 v1 = verts[( int )tri[1]];
				glm::vec3 v2 = verts[( int )tri[2]];

				// Perform ray/triangle intersection test.
				float tmp_t = INFINITYY;
				float tmp_u = 0.0f;
				float tmp_v = 0.0f;
				glm::vec3 tmp_normal( 0.0f, 0.0f, 0.0f );
				bool intersects_tri = Intersections::triIntersect( ray_o, ray_dir, v0, v1, v2, tmp_t, tmp_u, tmp_v);

				if ( intersects_tri ) {
					intersection_detected = true;
					if ( tmp_t < t ) {
						t = tmp_t;
						tri_index = triIndex;
						u = tmp_u;
						v = tmp_v;
					}
				}
			}

			return intersection_detected;
		}
		// Else, recurse.
		else {
			bool hit_left = false, hit_right = false;
			if ( curr_node->left ) {
				hit_left = intersect( curr_node->left, ray_o, ray_dir, t, tri_index, u, v);
			}
			if ( curr_node->right ) {
				hit_right = intersect( curr_node->right, ray_o, ray_dir, t, tri_index, u, v);
			}
			return hit_left || hit_right;
		}
	}

	return false;
}



////////////////////////////////////////////////////
// Debug methods.
////////////////////////////////////////////////////

void KDTreeCPU::printNumTrianglesInEachNode( KDTreeNode *curr_node, int curr_depth )
{
	std::cout << "Level: " << curr_depth << ", Triangles: " << curr_node->num_tris << std::endl;

	if ( curr_node->left ) {
		printNumTrianglesInEachNode( curr_node->left, curr_depth + 1 );
	}
	if ( curr_node->right ) {
		printNumTrianglesInEachNode( curr_node->right, curr_depth + 1 );
	}
}

void KDTreeCPU::printNodeIdsAndBounds( KDTreeNode *curr_node )
{
	std::cout << "Node ID: " << curr_node->id << std::endl;
	std::cout << "Node bbox min: ( " << curr_node->bbox.min.x << ", " << curr_node->bbox.min.y << ", " << curr_node->bbox.min.z << " )" << std::endl;
	std::cout << "Node bbox max: ( " << curr_node->bbox.max.x << ", " << curr_node->bbox.max.y << ", " << curr_node->bbox.max.z << " )" << std::endl;
	std::cout << std::endl;

	if ( curr_node->left ) {
		printNodeIdsAndBounds( curr_node->left );
	}
	if ( curr_node->right ) {
		printNodeIdsAndBounds( curr_node->right );
	}
}