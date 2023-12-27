#include "KDTreeStructs.h"
#include <iostream>


////////////////////////////////////////////////////
// KDTreeNode.
////////////////////////////////////////////////////

KDTreeNode::KDTreeNode()
{
	left = NULL;
	right = NULL;
	is_leaf_node = false;
	for ( int i = 0; i < 6; ++i ) {
		ropes[i] = NULL;
	}
	id = -99;
}

KDTreeNode::~KDTreeNode()
{
	if ( num_tris > 0 ) {
		delete[] tri_indices;
	}

	if ( left ) {
		delete left;
	}
	if ( right ) {
		delete right;
	}
}