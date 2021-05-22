#include "brdf.glsl"

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

vec3 ImportanceSampleDiffuse(vec3 N, samplerCube cubemap)
{
    vec4 result = vec4(0.0);

    float cubeWidth = float(textureSize(cubemap, 0).x);
    float solidAngleTexel = 4.0 * PI / (6.0 * cubeWidth * cubeWidth);

    // local basis for provided surface-normal
    mat3 local_frame = local_frame(N);

    const uint numSamples = 1024;
    for (uint i = 0; i < numSamples; ++i)
    {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 L = local_frame * sample_cosine(Xi);

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

    // local basis for provided surface-normal
    mat3 local_frame = local_frame(N);

    const uint numSamples = 1024;

    for (uint i = 0; i < numSamples; ++i)
    {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 H = local_frame * sample_GGX(Xi, roughness);
        vec3 L = reflect(-V, H);//2.0 * dot(V, H) * H - V;

        float NoL = max(dot(N, L), 0.0);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);

        if(NoL > 0.0)
        {
            float D = GTR2(NoH, roughness);
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
    roughness = max(0.001, roughness);

    vec3 N = vec3(0.0, 0.0, 1.0);
    vec3 V = vec3(sqrt(clamp(1.0 - NoV * NoV, 0.0, 1.0)), 0.0, NoV); // assuming isotropic BRDF

    float A = 0.0;
    float B = 0.0;

    // local basis for provided surface-normal
    mat3 local_frame = local_frame(N);

    const uint numSamples = 1024;

    for (uint i = 0; i < numSamples; ++i)
    {
        vec2 Xi = Hammersley(i, numSamples);
        vec3 H = local_frame * sample_GGX(Xi, roughness);
        vec3 L = reflect(-V, H);//2.0 * dot(V, H) * H - V;

        float NoL = clamp(L.z, 0.0, 1.0);
        float NoH = clamp(H.z, 0.0, 1.0);
        float VoH = clamp(dot(V, H), 0.0, 1.0);

        if (NoL > 0.0)
        {
            float G = G_Smith(roughness, NoV, NoL);
            float G_Vis = G * VoH / (NoH * NoV);
            float Fc = SchlickFresnel(VoH);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return vec2(A, B) / float(numSamples);
}