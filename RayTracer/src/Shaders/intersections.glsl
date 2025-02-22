float RayBBIntersection(in BoundingBox bbox, in Ray ray)
{
    vec3 rayDirInv = 1.0 / ray.Direction;
	vec3 l1 = (bbox.center - ray.Origin) * rayDirInv;
	vec3 l2 = bbox.extends * rayDirInv;

	vec3 tMin = l1 - l2;
	vec3 tMax = l1 + l2;

	vec3 t2 = max(tMin, tMax);
	float tFar = min(min(t2.x, t2.y), t2.z);

	if (tFar < 0.0f)
		return FLT_MAX;

	vec3 t1 = min(tMin, tMax);
	float tNear = max(max(t1.x, t1.y), t1.z);

	if (tNear > tFar)
		return FLT_MAX;

	return tNear;
}

bool RayTriangleIntersection(in TriangleOptimized tri, in Ray ray, out BVHHitInfo hitInfo)
{
	vec3 h = cross(ray.Direction, tri.e2);
	float a = dot(tri.e1, h);

	if (a > -FLT_EPSILON && a < FLT_EPSILON) {
		return false;
	}

	float f = 1.0 / a;
	vec3 s = ray.Origin - tri.v0;
	float u = f * dot(s, h);

	if (u < 0.0 || u > 1.0) {
		return false;
	}

	vec3 q = cross(s, tri.e1);
	float v = f * dot(ray.Direction, q);

	if (v < 0.0 || u + v > 1.0) {
		return false;
	}

	// at this stage we can compute t to find out where the intersection point is on the line
	float t = f * dot(tri.e2, q);

	if (t > FLT_EPSILON) { // ray intersection
		hitInfo.dist = t;
		hitInfo.u = u;
		hitInfo.v = v;
		return true;
	}

	return false;
}





HitInfo IntersectRay(in Ray ray)
{
	BVHHitInfo hitInfo = BVHHitInfo(FLT_MAX - 1.0, 0.0, 0.0, -1);
	int nodeStack[MAX_DEPTH];
	int stackIndex = 0;
	nodeStack[stackIndex++] = 0;
    
    vec3 n0, n1, n2; 
    vec2 uv0, uv1, uv2; 

	while (stackIndex > 0) {
		Node currentNode = GetNode(nodeStack[--stackIndex]);

		bool isLeaf = (currentNode.triangleCount > 0);
		if (isLeaf) {
			//Check all triangles in leaf
			BVHHitInfo triHit = BVHHitInfo(FLT_MAX - 1.0, 0.0, 0.0, -1);
			for (int i = currentNode.index; i < currentNode.index + currentNode.triangleCount; i++) {
                TriangleOptimized tri = GetTriangleOptimized(i);

				bool didHit = RayTriangleIntersection(tri, ray, triHit);
				if (didHit && triHit.dist < hitInfo.dist) {
					hitInfo.triIndex = i;
					hitInfo.dist = triHit.dist;
					hitInfo.u = triHit.u;
					hitInfo.v = triHit.v;

                    n0 = tri.n0;
                    n1 = tri.n1;
                    n2 = tri.n2;

                    uv0 = tri.uv0;
                    uv1 = tri.uv1;
                    uv2 = tri.uv2;
				}
			}
		}
		else {
			int childIndexA = currentNode.index;
			int childIndexB = currentNode.index + 1;

			float dstA = RayBBIntersection(GetNodeBB(childIndexA), ray);
			float dstB = RayBBIntersection(GetNodeBB(childIndexB), ray);

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
	

	HitInfo returnHit = HitInfo(vec3(0), vec3(0), vec2(0), -1);


	//If we hit something, work out the position, normal and material
	if (hitInfo.triIndex >= 0) {
        returnHit.materialIndex = tris_mats[hitInfo.triIndex];
		returnHit.position = ray.Origin + ray.Direction * hitInfo.dist;
        float w = (1.0 - hitInfo.u - hitInfo.v);
        returnHit.normal = w * n0 + hitInfo.u * n1 + hitInfo.v * n2;
        returnHit.uv = fract(w * uv0 + hitInfo.u * uv1 + hitInfo.v * uv2);
	}

	return returnHit;
}