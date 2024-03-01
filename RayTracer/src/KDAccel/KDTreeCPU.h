#ifndef KD_TREE_CPU_H
#define KD_TREE_CPU_H


#include <limits>
#include "KDTreeStructs.h"


////////////////////////////////////////////////////
// Constants.
////////////////////////////////////////////////////

const int NUM_TRIS_PER_NODE = 10;
const int MAX_DEPTH = 40;
const bool USE_TIGHT_FITTING_BOUNDING_BOXES = false;
const float INFINITYY = std::numeric_limits<float>::max();


////////////////////////////////////////////////////
// KDTreeCPU.
////////////////////////////////////////////////////
class KDTreeCPU
{
public:
	KDTreeCPU( int num_tris, glm::uvec3 *tris, int num_verts, glm::vec3 *verts );
	~KDTreeCPU( void );

	// Public traversal method that begins recursive search.
	bool intersect( const glm::vec3 &ray_o, const glm::vec3 &ray_dir, float &t, uint32_t& tri_index, float& u, float& v) const;	
	bool intersectStackless(const glm::vec3& ray_o, const glm::vec3& ray_dir, float& t, uint32_t& tri_index, float& u, float& v) const;

	// kd-tree getters.
	KDTreeNode* getRootNode( void ) const;
	int getNumLevels( void ) const;
	int getNumLeaves( void ) const;
	int getNumNodes( void ) const;

	// Input mesh getters.
	int getMeshNumVerts( void ) const;
	int getMeshNumTris( void ) const;
	glm::vec3* getMeshVerts( void ) const;
	glm::uvec3* getMeshTris( void ) const;

	// Debug methods.
	void printNumTrianglesInEachNode( KDTreeNode *curr_node, int curr_depth=1 );
	void printNodeIdsAndBounds( KDTreeNode *curr_node );

private:
	// kd-tree variables.
	KDTreeNode *root;
	int num_levels, num_leaves, num_nodes;

	// Input mesh variables.
	int num_verts, num_tris;
	glm::vec3 *verts;
	glm::uvec3* tris;

	KDTreeNode* constructTreeMedianSpaceSplit( int num_tris, int *tri_indices, boundingBox bounds, int curr_depth );

	KDTreeNode* constructTreeStackless(int num_tris, int *tri_indices, boundingBox bounds );

	// Private recursive traversal method.
	bool intersect( KDTreeNode *curr_node, const glm::vec3 &ray_o, const glm::vec3 &ray_dir, const glm::vec3& ray_dir_inv, float &t, uint32_t& tri_index, float& u, float& v) const;

	// Bounding box getters.
	SplitAxis getLongestBoundingBoxSide(const boundingBox& bbox);
	boundingBox computeTightFittingBoundingBox( int num_verts, glm::vec3 *verts );
	boundingBox computeTightFittingBoundingBox( int num_tris, int *tri_indices );

	// Triangle getters.
	float getMinTriValue( int tri_index, SplitAxis axis );
	float getMaxTriValue( int tri_index, SplitAxis axis );
};

#endif