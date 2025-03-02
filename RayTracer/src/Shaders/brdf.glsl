
// Helper: Smith geometry term using separable G1 functions.
float smithG1(vec3 v, vec3 n, float roughness) {
    float NoV = max(dot(n, v), 0.0);
    // Compute tanTheta via the identity tan²(theta) = (1 - cos²(theta)) / cos²(theta)
    float tanTheta = sqrt(max(1.0 - NoV * NoV, 0.0)) / (NoV + 1e-4);
    float alpha = roughness * roughness;
    float a = 1.0 / (alpha * tanTheta + 1e-4);
    // Empirical fit for GGX geometry shadowing.
    if (a < 1.6)
        return (3.535 * a + 2.181 * a * a) / (1.0 + 2.276 * a + 2.577 * a * a);
    return 1.0;
}

float smithG(vec3 v, vec3 l, vec3 n, float roughness) {
    return smithG1(v, n, roughness) * smithG1(l, n, roughness);
}

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

    // Convert roughness to alpha. Many implementations use alpha = roughness^2.
    float alpha = roughness * roughness;

    // Compute the azimuthal angle phi.
    float phi = 2.0 * PI * r1;

    // Sample the elevation angle theta using the GGX distribution.
    // One common inversion method uses:
    // tan²(theta) = alpha² * u2 / (1 - u2)
    float tanTheta2 = alpha * alpha * r2 / max(1.0 - r2, 1e-6);
    float cosTheta = 1.0 / sqrt(1.0 + tanTheta2);
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

    // Convert spherical coordinates (theta, phi) into Cartesian coordinates in tangent space.
    vec3 h_tangent = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Construct an orthonormal basis (tangent, bitangent, normal) around the surface normal.
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangentX = normalize(cross(up, N));
    vec3 tangentY = cross(N, tangentX);

    // Transform the half-vector from tangent space to world space.
    vec3 halfVector = tangentX * h_tangent.x + tangentY * h_tangent.y + N * h_tangent.z;
    return normalize(halfVector);
}




struct BRDFSample {
    vec3 direction;  // The new ray direction from the hit point.
    vec3 value;      // The BRDF evaluation (color contribution).
    float pdf;       // The probability density for the sampled direction.
};

BRDFSample sampleBRDF(vec3 incident, vec3 normal, Material mat, inout uint randomSeed) {
    BRDFSample brdfSample;

    // Compute the base reflectivity F0:
    // For dielectrics use a constant (e.g., 0.04), for metals use the albedo.
    vec3 F0 = mix(vec3(0.04), mat.Albedo.rgb, mat.Metallic);

    // Schlick's approximation for Fresnel factor.
    float cosTheta = max(dot(-incident, normal), 0.0);
    vec3 Fresnel = F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);

    // For metallic materials, force specular branch (i.e. probability 1).
    // Use the maximum channel of Fresnel as the probability to choose the specular branch.
    float specularProbability = 
        mat.Metallic > 0.0
        ? 1.0 
        : mat.Specular.r * clamp(max(Fresnel.r, max(Fresnel.g, Fresnel.b)), 0.0, 1.0);

    // If the random number falls in the specular range, or the material is metallic,
    // sample the specular component. Otherwise, sample the diffuse component.
    if (RandomValue(randomSeed) < specularProbability) {
        // --- Specular Branch ---
        // Sample a microfacet half-vector using GGX
        vec3 halfVec = sampleGGX(normal, mat.Roughness, randomSeed);

        // Reflect the incident ray about the half-vector
        vec3 reflected = reflect(incident, halfVec);
        if (dot(reflected, normal) < 0.0)
            reflected = -reflected;  // keep the reflection above the surface

        // Compute dot products needed:
        float NdotV = max(dot(normal, -incident), 0.0);
        float NdotL = max(dot(normal, reflected), 0.0);
        float NdotH = max(dot(normal, halfVec), 0.0);
        float VdotH = max(dot(-incident, halfVec), 0.0);

        // Convert roughness to alpha
        float alpha = mat.Roughness * mat.Roughness;

        // GGX normal distribution D(h)
        float D = GGX_Distribution(halfVec, normal, mat.Roughness);

        // Smith's geometry term G(v, l)
        float G = smithG(-incident, reflected, normal, mat.Roughness);

        float denominator = 4.0 * (NdotV * NdotL + 1e-4);
        vec3 specularBRDF = (D * Fresnel * G) / denominator;

        // Compute PDF for reflection
        // Typically: pdf = [ D * NdotH ] / [4 * VdotH] for half-vector sampling
        float pdf = D * NdotH / (4.0 * VdotH + 1e-4);

        brdfSample.direction = reflected;
        brdfSample.value     = specularBRDF / max(specularProbability, 1e-4); // Divide by the branch probability to keep the estimator unbiased
        brdfSample.pdf       = pdf;
    } else {
        // --- Diffuse Branch ---
        // Cosine-weighted hemisphere sampling for diffuse reflection.
        vec3 diffuseDir = RandomCosineInHemisphere(normal, randomSeed);

        // PDF for cosine-weighted sampling.
        float pdf = max(dot(diffuseDir, normal), 0.0) * PI1;

        // Lambertian diffuse BRDF: albedo/PI.
        vec3 diffuseBRDF = mat.Albedo.rgb * PI1;

        brdfSample.direction = diffuseDir;
        brdfSample.value = diffuseBRDF;
        brdfSample.pdf = pdf;
    }

    return brdfSample;
}






vec3 evaluateBRDF(vec3 incident, vec3 normal, vec3 outDir, Material mat) {
    // Convert the incident ray direction (from the camera) to a view direction.
    vec3 v = -incident; // view direction
    vec3 l = outDir;    // light direction

    float NoV = max(dot(normal, v), 0.0);
    float NoL = max(dot(normal, l), 0.0);
    if (NoV <= 0.0 || NoL <= 0.0)
        return vec3(0.0);

    // Compute the half vector between view and light directions.
    vec3 h = normalize(v + l);
    float NoH = max(dot(normal, h), 0.0);
    float VoH = max(dot(v, h), 0.0);

    // --- Specular Component ---
    // Convert roughness to alpha (often alpha = roughness^2).
    float roughness = mat.Roughness;
    float alpha = roughness * roughness;
    
    // Microfacet normal distribution (GGX/Trowbridge-Reitz)
    float D = GGX_Distribution(h, normal, roughness);
    
    // Geometry term using the Smith formulation.
    float G = smithG(v, l, normal, roughness);
    
    // Fresnel term using Schlick's approximation.
    vec3 F0 = mix(vec3(0.04), mat.Albedo.rgb, mat.Metallic);
    vec3 F = F0 + (vec3(1.0) - F0) * pow(1.0 - VoH, 5.0);
    
    // Specular BRDF term.
    vec3 specular = (D * F * G) / (4.0 * NoV * NoL + 1e-4);

    // --- Diffuse Component ---
    // For non-metal materials, include a diffuse term.
    // Metals (delta or near-delta specular materials) have no diffuse lobe.
    vec3 diffuse = vec3(0.0);
    if (mat.Metallic < 1.0) {
        // Lambertian diffuse model (albedo/PI), scaled by (1 - metallic).
        diffuse = (1.0 - mat.Metallic) * (mat.Albedo.rgb / PI);
    }

    // Combine the specular and diffuse contributions.
    return specular + diffuse;
}



// Evaluate the probability density (PDF) for sampling a given outgoing direction 'outDir'
// given the incident direction (from the camera) 'incident', the surface 'normal',
// and the material properties in 'mat'.
float evaluateBSDFPdf(vec3 incident, vec3 normal, vec3 outDir, Material mat) {
    // Convert the incident ray direction into the incoming light direction.
    // (Assuming 'incident' is the ray direction from the camera, so the actual incoming direction is -incident.)
    vec3 wi = -incident;
    // Compute the cosine of the angle between the incoming light and the surface normal.
    float cosIncident = max(dot(wi, normal), 0.0);

    // Compute the base reflectivity F0. For dielectrics, use a default (e.g., 0.04), and for metals use the albedo.
    vec3 F0 = mix(vec3(0.04), mat.Albedo.rgb, mat.Metallic);
    // Schlick's approximation for the Fresnel term.
    vec3 Fresnel = F0 + (vec3(1.0) - F0) * pow(1.0 - cosIncident, 5.0);
    // Use the maximum channel as the probability to choose the specular branch.
    float specProb = clamp(max(Fresnel.r, max(Fresnel.g, Fresnel.b)), 0.0, 1.0);

    // --- Diffuse PDF ---
    // For cosine-weighted hemisphere sampling, the PDF is: pdf = cos(theta) / PI.
    float pdf_diff = max(dot(outDir, normal), 0.0) / PI;

    // --- Specular PDF ---
    // Compute the half vector between the incoming and outgoing directions.
    vec3 halfVec = normalize(wi + outDir);
    // Calculate the GGX normal distribution function for the half vector.
    float D = GGX_Distribution(halfVec, normal, mat.Roughness);
    // Convert the half-vector PDF to the outgoing direction's PDF.
    // The factor 4 * dot(outDir, half) accounts for the change of variables.
    float pdf_spec = D * max(dot(normal, halfVec), 0.0) / (4.0 * max(dot(outDir, halfVec), 0.0) + 1e-4);

    // If the material is metallic, the diffuse component is zero and only the specular branch is used.
    if (mat.Metallic > 0.0)
        return pdf_spec;
    else
        // For a mixed material, return the weighted sum of the specular and diffuse PDFs.
        return specProb * pdf_spec + (1.0 - specProb) * pdf_diff;
}