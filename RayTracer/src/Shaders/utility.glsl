#define PI 3.14159265358
#define PI1 0.31830988618 // 1 / pi
#define PI1_2 0.15915494309 // 1 / (2 * pi)


#define FLT_MAX 3.402823466e+38
#define FLT_EPSILON 0.00001


// ==============================================================
// =====================    TONE MAPPING     ====================
// ==============================================================
const float ACE_a = 2.51f;
const float ACE_b = 0.03f;
const float ACE_c = 2.43f;
const float ACE_d = 0.59f;
const float ACE_e = 0.14f;

vec3 ACESFilm(vec3 x)
{
    vec3 aceColor = clamp((x * (ACE_a * x + ACE_b)) / (x * (ACE_c * x + ACE_d) + ACE_e), 0.0, 1.0);
    return aceColor;
}

vec3 LinearToSRGB(vec3 color) {
    color = clamp(color, 0.0f, 1.0f);
    return mix(
        1.055f * pow(color, vec3(1.0 / 2.4)) - 0.055,
        color * 12.92,
        lessThan(color, vec3(0.0031308))
    );
}

// ==============================================================
// =====================   TRANSFORMATIONS   ====================
// ==============================================================
mat3 computeTangentSpace(vec3 N) {
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return mat3(tangent, bitangent, N);
}

vec3 toWorld(vec3 localDir, vec3 N) {
    return computeTangentSpace(N) * localDir;
}

vec2 rotate(vec2 v, float a) {
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, s, -s, c);
	return m * v;
}

// ==============================================================
// =====================     RANDOM GEN      ====================
// ==============================================================
uint NextRandom(inout uint state)
{
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}

float RandomValue(inout uint state)
{
    return NextRandom(state) / 4294967295.0; // 2^32 - 1
}

vec3 RandomCosineInHemisphere(vec3 N, inout uint randomstate) {
    float r1 = RandomValue(randomstate);
    float r2 = RandomValue(randomstate);

    float theta = acos(sqrt(r1));
    float phi = 2.0 * PI * r2;

    vec3 localDir = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
    return toWorld(localDir, N); // Convert to world space
}

vec2 RandomInCircle(inout uint state) {
    float angle = RandomValue(state) * 2.0 * PI;
    return vec2(cos(angle), sin(angle)) * sqrt(RandomValue(state));
}


// ==============================================================
// =====================  MATERIAL GETTERS   ====================
// ==============================================================
vec4 GetAlbedoColor(in Material mat, in HitInfo hitInfo){
    vec4 albedoColor = mat.Albedo;
    int texIndex = textures_diffuseIdx[hitInfo.materialIndex*3];
    if (texIndex >= 0){
        int texWidth = textures_diffuseIdx[hitInfo.materialIndex*3 + 1];
        int texHeight = textures_diffuseIdx[hitInfo.materialIndex*3 + 2];

        int px_x = int(hitInfo.uv.x * (texWidth-1));
        int px_y = int((1.0 - hitInfo.uv.y) * (texHeight-1));

        int px_idx = texIndex + (px_y * texWidth + px_x);

        albedoColor = textures[px_idx];
    }
    return albedoColor;
}


vec4 GetSpecularColor(in Material mat, in HitInfo hitInfo){
    vec4 specularColor = mat.Specular;
    int texIndex = textures_specularIdx[hitInfo.materialIndex*3];
    if (texIndex >= 0){
        int texWidth = textures_specularIdx[hitInfo.materialIndex*3 + 1];
        int texHeight = textures_specularIdx[hitInfo.materialIndex*3 + 2];

        int px_x = int(hitInfo.uv.x * (texWidth-1));
        int px_y = int((1.0 - hitInfo.uv.y) * (texHeight-1));

        int px_idx = texIndex + (px_y * texWidth + px_x);

        specularColor = textures[px_idx];
    }
    return specularColor;
}



// ==============================================================
// =====================    HDRI SAMPLING    ====================
// ==============================================================
int binarySearchCDF(float r) {
    r *= 0.99999;

    int low = 0;
    int high = int(hdri_width) * int(hdri_height);
    int mid;

    // Loop until low and high converge.
    while (low < high) {
        mid = (low + high) / 2;
        if (hdri_cdf[mid] < r)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

vec3 hdrPixelToWorldDirection(in vec2 uv) {
    float longitude = (uv.x - 0.5) * 2.0 * PI; // Map [0,1] → [-π, π]
    float latitude  = (0.5 - uv.y) * PI;       // Map [0,1] → [π/2, -π/2]

    return normalize(vec3(
        cos(latitude) * cos(longitude),
        sin(latitude),
        cos(latitude) * sin(longitude)));
}


vec3 RotateSkyWorldDir(in vec3 worldDir){
    // Apply longitude rotation (horizontal)
    worldDir.xz = rotate(worldDir.xz, camdata.skyX * 2.0 * PI);

    vec3 rotatedDir;
    rotatedDir.x = worldDir.x;
    rotatedDir.y = worldDir.y * skyDir_cl - worldDir.z * skyDir_sl;
    rotatedDir.z = worldDir.y * skyDir_sl + worldDir.z * skyDir_cl;

    return normalize(rotatedDir);
}

vec3 RotateSkyWorldDirInv(in vec3 worldDir){
    // Apply longitude rotation (horizontal)
    worldDir.xz = rotate(worldDir.xz, -camdata.skyX * 2.0 * PI);

    vec3 rotatedDir;
    rotatedDir.x = worldDir.x;
    rotatedDir.y = worldDir.y * skyDirInv_cl - worldDir.z * skyDirInv_sl;
    rotatedDir.z = worldDir.y * skyDirInv_sl + worldDir.z * skyDirInv_cl;

    return normalize(rotatedDir);
}

vec4 SampleHDRIFromDirection(in vec3 worldDir)
{
    vec3 rotatedDir = RotateSkyWorldDir(worldDir);

    // Compute spherical coordinates
    float longitude = atan(rotatedDir.z, rotatedDir.x);  // Ranges from -pi to pi
    float latitude = asin(rotatedDir.y);                 // Ranges from -pi/2 to pi/2

	// Convert to normalized UV coordinates
	float u = (longitude + PI) * PI1_2; // Map from [-pi, pi] to [0,1]
	float v = (0.5 * PI - latitude) * PI1;    // Map from [-pi/2, pi/2] to [0,1], flip v

	// Convert to pixel coordinates
    ivec2 pixelCoords = ivec2(u * int(hdri_width), v * int(hdri_height));
	return imageLoad(hdri_image, pixelCoords);
}










float pdfBSDF(vec3 sampledDir, vec3 normal) {
    float cosTheta = max(dot(normal, sampledDir), 0.0);
    return cosTheta * PI1; // Lambertian PDF
}      

float pdfPointLight(vec3 lightPos, vec3 hitPoint) {
    vec3 lightDir = normalize(lightPos - hitPoint);
    float dist2 = dot(lightPos - hitPoint, lightPos - hitPoint);
    
    return 1.0 / (4.0 * PI * dist2); // Uniform sphere distribution
}