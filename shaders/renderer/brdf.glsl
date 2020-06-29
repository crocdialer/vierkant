#include "types.glsl"

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

//vec3 ImportanceSample(vec3 N)
//{
//    vec4 result = vec4(0.0);
//
//    float cubeWidth = float(textureSize(u_sampler_cube[0], 0).x);
//
//    const uint numSamples = 1024;
//    for (uint i = 0; i < numSamples; ++i)
//    {
//        vec2 Xi = Hammersley(i, numSamples);
//        vec3 L = ImportanceSampleCosine(Xi, N);
//
//        float NoL = max(dot(N, L), 0.0);
//
//        if (NoL > 0.0)
//        {
//            // Compute Lod using inverse solid angle and pdf.
//            // From Chapter 20.4 Mipmap filtered samples in GPU Gems 3.
//            // http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html
//            float pdf = NoL * ONE_OVER_PI;
//            float solidAngleTexel = 4.0 * PI / (6.0 * cubeWidth * cubeWidth);
//            float solidAngleSample = 1.0 / (numSamples * pdf);
//            float lod = 0.5 * log2(solidAngleSample / solidAngleTexel);
//
//            vec3 hdrRadiance = textureLod(u_sampler_cube[0], L, lod).rgb;
//            result += vec4(hdrRadiance / pdf, 1.0);
//        }
//    }
//    return result.rgb / result.w;
//}

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float map_roughness(float r)
{
    return mix(0.025, 0.975, r);
}

vec3 BRDF_Lambertian(vec3 color, float metalness)
{
	return mix(color, vec3(0.0), metalness) * ONE_OVER_PI;
}

vec3 F_schlick(vec3 f0, float u)
{
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float Vis_schlick(float ndotl, float ndotv, float roughness)
{
	// = G_Schlick / (4 * ndotv * ndotl)
	float a = roughness + 1.0;
	float k = a * a * 0.125;

	float Vis_SchlickV = ndotv * (1 - k) + k;
	float Vis_SchlickL = ndotl * (1 - k) + k;

	return 0.25 / (Vis_SchlickV * Vis_SchlickL);
}

float D_GGX(float NoH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = NoH * NoH * (a2 - 1.0) + 1.0;
	denom = 1.0 / (denom * denom);
	return a2 * denom * ONE_OVER_PI;
}

vec4 shade(in lightsource_t light, in vec3 normal, in vec3 eyeVec, in vec4 base_color,
           float roughness, float metalness, float shade_factor)
{
    roughness = map_roughness(roughness);
    vec3 lightDir = light.type > 0 ? (light.position - eyeVec) : -light.direction;
    vec3 L = normalize(lightDir);
    vec3 E = normalize(-eyeVec);
    vec3 H = normalize(L + E);

    // vec3 ambient = light.ambient.rgb;
    float nDotL = max(0.f, dot(normal, L));
    float nDotH = max(0.f, dot(normal, H));
    float nDotV = max(0.f, dot(normal, E));
    float lDotH = max(0.f, dot(L, H));
    float att = shade_factor;

    if(light.type > 0)
    {
        // distance^2
        float dist2 = dot(lightDir, lightDir);
        float v = dist2 / (light.radius * light.radius);
        v = clamp(1.f - v * v, 0.f, 1.f);
        att *= v * v / (1.f + dist2 * light.quadraticAttenuation);

        if(light.type > 1)
        {
            float spot_effect = dot(normalize(light.direction), -L);
            att *= spot_effect < light.spotCosCutoff ? 0 : 1;
            spot_effect = pow(spot_effect, light.spotExponent);
            att *= spot_effect;
        }
    }

    // brdf term
    const vec3 dielectricF0 = vec3(0.04);
    vec3 f0 = mix(dielectricF0, base_color.rgb, metalness);
    vec3 F = F_schlick(f0, lDotH);
    float D = D_GGX(nDotH, roughness);
    float Vis = Vis_schlick(nDotL, nDotV, roughness);
    vec3 Ir = light.diffuse.rgb * light.intensity;
    vec3 diffuse = BRDF_Lambertian(base_color.rgb, metalness);
    vec3 specular = F * D * Vis;
    return vec4((diffuse + specular) * nDotL * Ir * att, 1.0);
}
