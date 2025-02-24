struct Ray
{
	vec3 Origin;
	vec3 Direction;
};

struct BoundingBox
{
    vec3 center;
    vec3 extends;
};

struct Node
{
	BoundingBox boundingBox;
	int index;
	int triangleCount;
};

struct HitInfo {
	vec3 position;
	vec3 normal;
    vec2 uv;
	int materialIndex;
};

struct BVHHitInfo {
	float dist;
	float u;
	float v;
	int triIndex;
};

struct TriangleOptimized
{
	vec3 v0;
	vec3 e1;
	vec3 e2;
	vec3 n0;
	vec3 n1;
	vec3 n2;
	vec2 uv0;
	vec2 uv1;
	vec2 uv2;
};

struct Material {
	vec4 Albedo;
	vec4 Emission;
	float Metallic;
	float Roughness;
	float Transparency;
	float IOR;
};



// --------------------------------------------
// -------------- Getters ---------------------
// --------------------------------------------

TriangleOptimized GetTriangleOptimized(uint triIndex){
    uint triOffset = 24 * triIndex;

    vec3 v0 = vec3(
    tris_opt[triOffset],
    tris_opt[triOffset+ 1],
    tris_opt[triOffset+ 2]);

    vec3 e1 = vec3(
    tris_opt[triOffset + 3],
    tris_opt[triOffset + 4],
    tris_opt[triOffset + 5]);

    vec3 e2 = vec3(
    tris_opt[triOffset + 6],
    tris_opt[triOffset + 7],
    tris_opt[triOffset + 8]);

    vec3 n0 = vec3(
    tris_opt[triOffset + 9],
    tris_opt[triOffset + 10],
    tris_opt[triOffset + 11]);

    vec3 n1 = vec3(
    tris_opt[triOffset + 12],
    tris_opt[triOffset + 13],
    tris_opt[triOffset + 14]);

    vec3 n2 = vec3(
    tris_opt[triOffset + 15],
    tris_opt[triOffset + 16],
    tris_opt[triOffset + 17]);

    
    vec2 uv0 = vec2(
    tris_opt[triOffset + 18],
    tris_opt[triOffset + 19]);

    vec2 uv1 = vec2(
    tris_opt[triOffset + 20],
    tris_opt[triOffset + 21]);

    vec2 uv2 = vec2(
    tris_opt[triOffset + 22],
    tris_opt[triOffset + 23]);

    return TriangleOptimized(v0, e1, e2, n0, n1, n2, uv0, uv1, uv2);
}


Material GetMaterial(uint materialIndex){
    uint matOffset = 12 * materialIndex;

    vec4 albedo = vec4(
    mats[matOffset],
    mats[matOffset+ 1],
    mats[matOffset+ 2],
    mats[matOffset+ 3]);

    vec4 emission = vec4(
    mats[matOffset + 4],
    mats[matOffset + 5],
    mats[matOffset + 6],
    mats[matOffset + 7]);

    float metallic = mats[matOffset + 8];
    float roughness = mats[matOffset + 9];
    float transparency = mats[matOffset + 10];
    float ior = mats[matOffset + 11];

    return Material(albedo, emission, metallic, roughness, transparency, ior);
}

BoundingBox GetNodeBB(uint nodeIndex){
    uint bbOffset = 6 * nodeIndex;

    vec3 center = vec3(
    nodes_bb[bbOffset],
    nodes_bb[bbOffset + 1],
    nodes_bb[bbOffset + 2]);

    vec3 extends = vec3(
    nodes_bb[bbOffset + 3],
    nodes_bb[bbOffset + 4],
    nodes_bb[bbOffset + 5]);

    return BoundingBox(center, extends);
}

Node GetNode(int nodeIndex){
    Node node;
    node.boundingBox = GetNodeBB(nodeIndex);
    node.index = nodes_index_tricount[nodeIndex*2];
    node.triangleCount = nodes_index_tricount[nodeIndex*2 + 1];
    return node;
}
