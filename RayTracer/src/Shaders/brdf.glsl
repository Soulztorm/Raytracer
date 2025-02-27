// Returns the GGX normal distribution value for a given half-vector 'h',
// surface normal 'n', and roughness parameter 'roughness'.
// The roughness is converted to an alpha value by squaring it.
float GGX_Distribution(vec3 h, vec3 n, float roughness) {
    // Convert roughness to alpha (using roughness^2 is common for energy conservation)
    float alpha = roughness * roughness;
    // Calculate the cosine of the angle between the normal and the half-vector.
    float NoH = max(dot(n, h), 0.0);
    
    // Compute alpha squared.
    float alpha2 = alpha * alpha;
    
    // Denominator of the GGX distribution function.
    // The expression ((n.h)^2 * (alpha^2 - 1) + 1)^2 ensures proper normalization.
    float denom = (NoH * NoH * (alpha2 - 1.0) + 1.0);
    
    // Return the GGX distribution, with a small epsilon to avoid division by zero.
    return alpha2 / (PI * denom * denom + 1e-6);
}


vec3 sampleGGX(vec3 N, float roughness, inout uint randomSeed) {
    float r1 = RandomValue(randomSeed); // Random number in [0,1]
    float r2 = RandomValue(randomSeed);

    float alpha = roughness * roughness; // Convert roughness to alpha
    float theta = atan(sqrt((alpha * alpha * r1) / (1.0 - r1))); // GGX theta
    float phi = 2.0 * PI * r2; // Uniform phi

    // Convert spherical coordinates to Cartesian
    vec3 H = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

    // Transform to world space
    return toWorld(H, N);
}




struct BRDFSample {
    vec3 direction;  // The new ray direction from the hit point.
    vec3 value;      // The BRDF evaluation (color contribution).
    float pdf;       // The probability density for the sampled direction.
};

BRDFSample sampleBRDF(vec3 incident, vec3 normal, Material mat, vec4 albedoColor, inout uint randomSeed) {
    BRDFSample brdfSample;

    // Compute the base reflectivity F0:
    // For dielectrics use a constant (e.g., 0.04), for metals use the albedo.
    vec3 F0 = mix(vec3(0.04), albedoColor.rgb, mat.Metallic);

    // Schlick's approximation for Fresnel factor.
    float cosTheta = max(dot(-incident, normal), 0.0);
    vec3 Fresnel = F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);

    // Use the maximum channel of Fresnel as the probability to choose the specular branch.
    float specularProbability = clamp(max(Fresnel.r, max(Fresnel.g, Fresnel.b)), 0.0, 1.0);

    // If the random number falls in the specular range, or the material is metallic,
    // sample the specular component. Otherwise, sample the diffuse component.
    if (RandomValue(randomSeed) < specularProbability || mat.Metallic > 0.0) {
        // --- Specular Branch ---
        // Sample a microfacet half-vector using the GGX distribution.
        vec3 halfVec = sampleGGX(normal, mat.Roughness, randomSeed);

        // Reflect the incident ray about the half-vector.
        vec3 reflected = reflect(incident, halfVec);
        if (dot(reflected, normal) < 0.0)
            reflected = -reflected;  // Ensure the ray is above the surface.

        // Compute the GGX distribution term (D) for the half-vector.
        float D = GGX_Distribution(halfVec, normal, mat.Roughness);

        // Compute the PDF for specular reflection.
        // The factor 4*dot(reflected, half) comes from the change of variables.
        float pdf = D * max(dot(normal, halfVec), 0.0) / (4.0 * max(dot(reflected, halfVec), 0.0) + 1e-4);

        // Evaluate the specular BRDF (using a simplified Cook-Torrance model).
        vec3 specularBRDF = Fresnel * D / (4.0 * cosTheta + 1e-4);

        brdfSample.direction = reflected;
        brdfSample.value = specularBRDF;
        brdfSample.pdf = pdf;
    } else {
        // --- Diffuse Branch ---
        // Cosine-weighted hemisphere sampling for diffuse reflection.
        vec3 diffuseDir = RandomCosineInHemisphere(normal, randomSeed);

        // PDF for cosine-weighted sampling.
        float pdf = max(dot(diffuseDir, normal), 0.0) / PI;

        // Lambertian diffuse BRDF: albedo/PI.
        vec3 diffuseBRDF = albedoColor.rgb / PI;

        brdfSample.direction = diffuseDir;
        brdfSample.value = diffuseBRDF;
        brdfSample.pdf = pdf;
    }

    return brdfSample;
}