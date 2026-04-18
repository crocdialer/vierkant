module importance_sampling;

import "../utils/bsdf.slang";
import "../utils/sampling.slang";
import "../utils/constants.slang";

namespace renderer
{

float G1(float k, float NoV)
{
    return NoV / (NoV * (1.0 - k) + k);
}

float G_Smith(float roughness, float NoV, float NoL)
{
    float alpha = roughness * roughness;
    float k = alpha * 0.5;
    return G1(k, NoL) * G1(k, NoV);
}

public float3 ImportanceSampleDiffuse(float3 N, SamplerCube cubemap)
{
    float4 result = float4(0.0);

    uint cubeWidth, cubeHeight;
    cubemap.GetDimensions(cubeWidth, cubeHeight);
    float solidAngleTexel = 4.0 * utils::PI / (6.0 * float(cubeWidth) * float(cubeWidth));

    float3x3 local_frame = utils::local_frame(N);

    const uint numSamples = 1024;
    for(uint i = 0; i < numSamples; ++i)
    {
        float2 Xi = utils::Hammersley(i, numSamples);
        float3 L = mul(utils::sample_hemisphere_cosine(Xi), local_frame);

        float NoL = max(dot(N, L), 0.0);

        if(NoL > 0.0)
        {
            float pdf = NoL * utils::ONE_OVER_PI;
            float solidAngleSample = 1.0 / (numSamples * pdf);
            float lod = 0.5 * log2(solidAngleSample / solidAngleTexel);

            float3 hdrRadiance = cubemap.SampleLevel(L, lod).rgb;
            result += float4(hdrRadiance / pdf, 1.0);
        }
    }
    return result.rgb / result.w;
}

public float3 ImportanceSampleSpecular(float3 R, float roughness, SamplerCube cubemap)
{
    roughness = max(0.001, roughness * roughness);

    float3 N = R;
    float3 V = R;
    float4 result = float4(0.0);

    uint cubeWidth, cubeHeight;
    cubemap.GetDimensions(cubeWidth, cubeHeight);
    float solidAngleTexel = 4.0 * utils::PI / (6.0 * float(cubeWidth) * float(cubeWidth));

    float3x3 local_frame = utils::local_frame(N);

    const uint numSamples = 1024;
    for(uint i = 0; i < numSamples; ++i)
    {
        float2 Xi = utils::Hammersley(i, numSamples);
        float3 H = mul(utils::sample_GGX(Xi, roughness), local_frame);
        float3 L = reflect(-V, H);

        float NoL = max(dot(N, L), 0.0);
        float NoH = max(dot(N, H), 0.0);
        float VoH = max(dot(V, H), 0.0);

        if(NoL > 0.0)
        {
            float D = utils::GTR2(NoH, roughness);
            float pdf = D * NoH / (4.0 * VoH);
            float solidAngleSample = 1.0 / (numSamples * pdf);
            float lod = 0.5 * log2(solidAngleSample / solidAngleTexel);

            float3 hdrRadiance = cubemap.SampleLevel(L, lod).rgb;
            result += float4(hdrRadiance * NoL, NoL);
        }
    }
    if(result.w == 0.0) { return result.rgb; }
    else { return result.rgb / result.w; }
}

public float2 IntegrateBRDF(float roughness, float NoV)
{
    roughness = max(0.001, roughness);
    float a2 = roughness * roughness;

    float3 N = float3(0.0, 0.0, 1.0);
    float3 V = float3(sqrt(clamp(1.0 - NoV * NoV, 0.0, 1.0)), 0.0, NoV);

    float A = 0.0;
    float B = 0.0;

    float3x3 local_frame = utils::local_frame(N);

    const uint numSamples = 1024;
    for(uint i = 0; i < numSamples; ++i)
    {
        float2 Xi = utils::Hammersley(i, numSamples);
        float3 H = mul(utils::sample_GGX(Xi, a2), local_frame);
        float3 L = reflect(-V, H);

        float NoL = clamp(L.z, 0.0, 1.0);
        float NoH = clamp(H.z, 0.0, 1.0);
        float VoH = clamp(dot(V, H), 0.0, 1.0);

        if(NoL > 0.0)
        {
            float G = G_Smith(roughness, NoV, NoL);
            float G_Vis = G * VoH / (NoH * NoV);
            float Fc = utils::SchlickFresnel(VoH);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return float2(A, B) / float(numSamples);
}

}// namespace renderer
