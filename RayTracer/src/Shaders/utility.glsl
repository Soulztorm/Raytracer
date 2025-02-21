#define PI 3.14159265358
#define PI1 0.31830988618 // 1 / pi
#define PI1_2 0.15915494309 // 1 / (2 * pi)


#define FLT_MAX 3.402823466e+38
#define FLT_EPSILON 0.00001


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

// Random value in normal distribution (with mean=0 and sd=1)
float RandomValueNormalDistribution(inout uint state)
{
    // Thanks to https://stackoverflow.com/a/6178290
    float theta = 2 * 3.1415926 * RandomValue(state);
    float rho = sqrt(-2 * log(RandomValue(state)));
    return rho * cos(theta);
}

// Calculate a random direction
vec3 RandomDirection(inout uint state)
{
    // Thanks to https://math.stackexchange.com/a/1585996
    float x = RandomValueNormalDistribution(state);
    float y = RandomValueNormalDistribution(state);
    float z = RandomValueNormalDistribution(state);
    return normalize(vec3(x, y, z));
}

vec3 RandomInUnitSphere(inout uint state)
{
    return normalize(vec3(RandomValue(state) * 2.0 - 1.0, RandomValue(state) * 2.0 - 1.0f, RandomValue(state) * 2.0 - 1.0f));
}

vec2 RandomInCircle(inout uint state) {
    float angle = RandomValue(state) * 2.0 * PI;
    return vec2(cos(angle), sin(angle)) * sqrt(RandomValue(state));
}

mat3 computeTangentSpace(vec3 N) {
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return mat3(tangent, bitangent, N);
}

vec3 toWorld(vec3 localDir, vec3 N) {
    return computeTangentSpace(N) * localDir;
}

vec3 RandomCosineInHemisphere(vec3 N, inout uint randomstate) {
    float r1 = RandomValue(randomstate);
    float r2 = RandomValue(randomstate);

    float theta = acos(sqrt(r1));
    float phi = 2.0 * PI * r2;

    vec3 localDir = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
    return toWorld(localDir, N); // Convert to world space
}




vec4 GetAlbedoColor(in Material mat, in HitInfo hitInfo){
    vec4 albedoColor = mat.Albedo;
    int texIndex = textures_diffuseIdx[hitInfo.materialIndex*3];
    if (texIndex >= 0){
        int texWidth = textures_diffuseIdx[hitInfo.materialIndex*3 + 1];
        int texHeight = textures_diffuseIdx[hitInfo.materialIndex*3 + 2];

        int px_x = int(hitInfo.uv.x * (texWidth - 1));
        int px_y = int((1.0 - hitInfo.uv.y) * (texHeight - 1));

        int px_idx = texIndex + (px_y * texWidth + px_x);

        albedoColor = textures_diffuse[px_idx];
    }
    return albedoColor;
}
           


