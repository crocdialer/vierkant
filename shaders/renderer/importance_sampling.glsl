#include "brdf.glsl"

vec2 Hammersley(uint i, uint N)
{
    float vdc = float(bitfieldReverse(i)) * 2.3283064365386963e-10; // Van der Corput
    return vec2(float(i) / float(N), vdc);
}

float G1(float k, float NoV)
{
    return NoV / (NoV * (1.0 - k) + k);
}

float G_Smith(float roughness, float NoV, float NoL)
{
    float alpha = roughness * roughness;
    float k = alpha * 0.5; // use k = (roughness + 1)^2 / 8 for analytic lights
    return G1(k, NoL) * G1(k, NoV);
}

vec3 ImportanceSampleCosine(vec2 Xi, vec3 N)
{
    float cosTheta = sqrt(max(1.0 - Xi.y, 0.0));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float phi = 2.0 * PI * Xi.x;

    vec3 L = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(N, up));
    vec3 bitangent = cross(N, tangent);

    return tangent * L.x + bitangent * L.y + N * L.z;
}

// Sample a half-vector in world space
vec3 ImportanceSampleGGX(vec2 Xi, float roughness, vec3 N)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
    float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));

    vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return tangent * H.x + bitangent * H.y + N * H.z;
}

vec3 ImportanceSampleDiffuse(vec3 N, samplerCube cubemap)
{
    vec4 result = vec4(0.0);

    float cubeWidth = float(textureSize(cubemap, 0).x);
    float solidAngleTexel = 4.0 * PI / (6.0 * cubeWidth * cubeWidth);

    const uint numSamples = 1024;
    for (uint i = 0; i < numSamples; ++i)
    {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 L = ImportanceSampleCosine(Xi, N);

        float NoL = max(dot(N, L), 0.0);

        if (NoL > 0.0)
        {
            // Compute Lod using inverse solid angle and pdf.
            // From Chapter 20.4 Mipmap filtered samples in GPU Gems 3.
            // http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html
            float pdf = NoL * ONE_OVER_PI;
            float solidAngleSample = 1.0 / (numSamples * pdf);
            float lod = 0.5 * log2(solidAngleSample / solidAngleTexel);

            vec3 hdrRadiance = textureLod(cubemap, L, lod).rgb;

            result += vec4(hdrRadiance / pdf, 1.0);
        }
    }
    return result.rgb / result.w;
}

vec3 ImportanceSampleSpecular(vec3 R, float roughness, samplerCube cubemap)
{
    // Approximation: assume V == R
    // We lose enlongated reflection by doing this but we also get rid
    // of one variable. So we are able to bake irradiance into a single
    // mipmapped cube map. Otherwise, a cube map array is required.
    vec3 N = R;
    vec3 V = R;
    vec4 result = vec4(0.0);

    float cubeWidth = float(textureSize(cubemap, 0).x);
    float solidAngleTexel = 4.0 * PI / (6.0 * cubeWidth * cubeWidth);

    const uint numSamples = 1024;

    for (uint i = 0; i < numSamples; ++i)
    {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 H = ImportanceSampleGGX(Xi, roughness, N);
        vec3 L = 2.0 * dot(V, H) * H - V;

        float NoL = max(dot(N, L), 0.0);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);

        if(NoL > 0.0)
        {
            float D = D_GGX(roughness, NoH);
            float pdf = D * NoH / (4.0 * VoH);
            float solidAngleSample = 1.0 / (numSamples * pdf);
            float lod = roughness == 0.0 ? 0.0 : 0.5 * log2(solidAngleSample / solidAngleTexel);

            vec3 hdrRadiance = textureLod(cubemap, L, lod).rgb;
            result += vec4(hdrRadiance * NoL, NoL);
        }
    }
    if(result.w == 0.0){ return result.rgb; }
    else{ return result.rgb / result.w;	}
}

vec2 IntegrateBRDF(float roughness, float NoV)
{
    vec3 N = vec3(0.0, 0.0, 1.0);
    vec3 V = vec3(sqrt(clamp(1.0 - NoV * NoV, 0.0, 1.0)), 0.0, NoV); // assuming isotropic BRDF

    float A = 0.0;
    float B = 0.0;

    const uint numSamples = 1024;

    for (uint i = 0; i < numSamples; ++i)
    {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 H = ImportanceSampleGGX(Xi, roughness, N);
        vec3 L = 2.0 * dot(V, H) * H - V;

        float NoL = clamp(L.z, 0.0, 1.0);
        float NoH = clamp(H.z, 0.0, 1.0);
        float VoH = clamp(dot(V, H), 0.0, 1.0);

        if (NoL > 0.0)
        {
            float G = G_Smith(roughness, NoV, NoL);

            float G_Vis = G * VoH / (NoH * NoV);
            float Fc = pow(1.0 - VoH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return vec2(A, B) / float(numSamples);
}