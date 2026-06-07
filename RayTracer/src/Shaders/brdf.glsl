
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

float V_SmithGGXCorrelated(float NoV, float NoL, float alpha) {
    float alpha2 = alpha * alpha;
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - alpha2) + alpha2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - alpha2) + alpha2);
    return 0.5 / (GGXV + GGXL + 1e-4);
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

BRDFSample sampleBRDF(vec3 incident, vec3 normal, vec3 Fresnel, float FresnelMax, float cosTheta, Material mat, inout uint randomSeed) {
    BRDFSample brdfSample;

    // For metallic materials, force specular branch (i.e. probability 1).
    // Use the maximum channel of Fresnel as the probability to choose the specular branch.
    float specularProbability = 
        mat.Metallic > 0.0
        ? 1.0 
        : mat.Specular.r * FresnelMax;

    // If the random number falls in the specular range, or the material is metallic,
    // sample the specular component. Otherwise, sample the diffuse component.
    if (RandomValue(randomSeed) < specularProbability) {
        // --- Specular Branch ---
        // Sample a microfacet half-vector using GGX
        vec3 halfVec = sampleGGX(normal, mat.Roughness, randomSeed);

        // Reflect the incident ray about the half-vector
        vec3 reflected = reflect(incident, halfVec);
        // Compute dot products needed:
        float NdotL = max(dot(normal, reflected), 0.0);

        // If the ray points inside the mesh, mirror it back above the surface
        if (NdotL < 0.0) {
            reflected = normalize(reflected - 2.0 * NdotL * normal);
            NdotL = max(dot(normal, reflected), 0.0); // Recalculate NdotL safely
        }

        float NdotH = max(dot(normal, halfVec), 0.0);
        float VdotH = max(dot(-incident, halfVec), 0.0);

        // Convert roughness to alpha
        float alpha = mat.Roughness * mat.Roughness;

        // GGX normal distribution D(h)
        float D = GGX_Distribution(halfVec, normal, alpha);

        // Smith's geometry term G(v, l)
        float V = V_SmithGGXCorrelated(cosTheta, NdotL, alpha);

       // Calculate Fresnel dynamically using VdotH
        vec3 F0 = mix(vec3(0.04), mat.Albedo.rgb, mat.Metallic);
        vec3 Fresnel_VH = F0 + (vec3(1.0) - F0) * pow(1.0 - VdotH, 5.0);

        // Use this new Fresnel for the BRDF evaluation
        vec3 specularBRDF = D * Fresnel_VH * V;

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
        //brdfSample.value = diffuseBRDF;
        // Divide by the probability of taking the diffuse branch (1.0 - specularProbability)
        brdfSample.value = diffuseBRDF / max(1.0 - specularProbability, 1e-4);

        brdfSample.pdf = pdf;
    }

    return brdfSample;
}






vec3 evaluateBRDF(vec3 incident, vec3 normal, vec3 outDir, float cosTheta, vec3 halfLightVec, Material mat) {
    // Convert the incident ray direction (from the camera) to a view direction.
    float NoL = max(dot(normal, outDir), 0.0);
    if (cosTheta <= 0.0 || NoL <= 0.0)
        return vec3(0.0);

    // Compute the half vector between view and light directions.
    float VoH = max(dot(-incident, halfLightVec), 0.0);

    // --- Specular Component ---
    // Convert roughness to alpha (often alpha = roughness^2).
    float alpha = mat.Roughness * mat.Roughness;
    
    // Microfacet normal distribution (GGX/Trowbridge-Reitz)
    float D = GGX_Distribution(halfLightVec, normal, alpha);
    
    // Use the Correlated Visibility function to match sampleBRDF
    float V = V_SmithGGXCorrelated(cosTheta, NoL, alpha);

    // Fresnel term using Schlick's approximation.
    vec3 F0 = mix(vec3(0.04), mat.Albedo.rgb, mat.Metallic);
    vec3 Fresnel = F0 + (vec3(1.0) - F0) * pow(1.0 - VoH, 5.0);
    
    // Specular BRDF term (V handles the division!)
    vec3 specular = D * Fresnel * V;

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
float evaluateBSDFPdf(vec3 incident, vec3 normal, vec3 outDir, float FresnelMax, vec3 halfLightVec, Material mat) {
    // Convert the incident ray direction into the incoming light direction.
    // (Assuming 'incident' is the ray direction from the camera, so the actual incoming direction is -incident.)

    // Use the maximum channel as the probability to choose the specular branch.
    float specProb = FresnelMax;

    // --- Diffuse PDF ---
    // For cosine-weighted hemisphere sampling, the PDF is: pdf = cos(theta) / PI.
    float pdf_diff = max(dot(outDir, normal), 0.0) / PI;

    // --- Specular PDF ---
    // Calculate the GGX normal distribution function for the half vector.
    float D = GGX_Distribution(halfLightVec, normal, mat.Roughness);
    // Convert the half-vector PDF to the outgoing direction's PDF.
    // The factor 4 * dot(outDir, half) accounts for the change of variables.
    float pdf_spec = D * max(dot(normal, halfLightVec), 0.0) / (4.0 * max(dot(outDir, halfLightVec), 0.0) + 1e-4);

    // If the material is metallic, the diffuse component is zero and only the specular branch is used.
    if (mat.Metallic > 0.0)
        return pdf_spec;
    else
        // For a mixed material, return the weighted sum of the specular and diffuse PDFs.
        return specProb * pdf_spec + (1.0 - specProb) * pdf_diff;
}