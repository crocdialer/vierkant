#define PI 3.1415926535897932384626433832795
#define ONE_OVER_PI 0.31830988618379067153776752674503

//! random point on a unit-disc
vec2 sample_unit_disc(vec2 Xi)
{
    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    return vec2(Xi.x * cos(theta), Xi.x * sin(theta));
}

//! random point on a unit-sphere
vec3 sample_unit_sphere(vec2 Xi)
{
    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    // [-1, 1]
    float u = 2.0 * Xi.x - 1.0;

    const float r = sqrt(1.0 - u * u);
    return vec3(r * cos(theta), r * sin(theta), u);
}

//! return a Hammersley point in range [0, 1]
vec2 Hammersley(uint i, uint N)
{
    float vdc = float(bitfieldReverse(i)) * 2.3283064365386963e-10; // Van der Corput
    return vec2(float(i) / float(N), vdc);
}

/*
 * Calculates local coordinate frame for a given normal
 */
mat3 local_frame(in vec3 normal)
{
    vec3 up = abs(normal.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangentX = normalize(cross(normal, up));
    vec3 tangentY = cross(normal, tangentX);
    return mat3(tangentX, tangentY, normal);
}

vec3 sample_cosine(vec2 Xi)
{
    float cosTheta = sqrt(max(1.0 - Xi.y, 0.0));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float phi = 2.0 * PI * Xi.x;

    // L
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Sample a half-vector in world space
vec3 sample_GGX(vec2 Xi, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
    float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));

    // H
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

/*
 * Schlick's approximation of the specular reflection coefficient R
 * (1 - cosTheta)^5
 */
float SchlickFresnel(float u)
{
    float m = clamp(1.0 - u, 0.0, 1.0);
    return m * m * m * m * m; // power of 5
}

/*
 * Generalized-Trowbridge-Reitz (D)
 * Describes differential area of microfacets for the surface normal
 */
float GTR2(float NDotH, float a)
{
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
    return a2 / (PI * t * t);
}

/*
 * Smith's geometric masking/shadowing function for GGX noraml distribution (G)
 */
float SmithGGX(float NDotv, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotv * NDotv;
    return 1.0 / (NDotv + sqrt(a + b - a * b));
}

//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//vec3 BRDF_Lambertian(vec3 color, float metalness)
//{
//	return mix(color, vec3(0.0), metalness) * ONE_OVER_PI;
//}
//
//// Schlick's fresnel approximation
//vec3 F_schlick(vec3 f0, float u)
//{
//    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
//}
//
//float Vis_schlick(float ndotl, float ndotv, float roughness)
//{
//	// = G_Schlick / (4 * ndotv * ndotl)
//	float a = roughness + 1.0;
//	float k = a * a * 0.125;
//
//	float Vis_SchlickV = ndotv * (1 - k) + k;
//	float Vis_SchlickL = ndotl * (1 - k) + k;
//
//	return 0.25 / (Vis_SchlickV * Vis_SchlickL);
//}
//
//float D_GGX(float NoH, float roughness)
//{
//	float a = roughness * roughness;
//	float a2 = a * a;
//	float denom = NoH * NoH * (a2 - 1.0) + 1.0;
//	denom = 1.0 / (denom * denom);
//	return a2 * denom * ONE_OVER_PI;
//}
//
//vec4 shade(in lightsource_t light, in vec3 normal, in vec3 eyeVec, in vec4 base_color,
//           float roughness, float metalness, float shade_factor)
//{
//    vec3 lightDir = light.type > 0 ? (light.position - eyeVec) : -light.direction;
//    vec3 L = normalize(lightDir);
//    vec3 E = normalize(-eyeVec);
//    vec3 H = normalize(L + E);
//
//    float nDotL = max(0.f, dot(normal, L));
//    float nDotH = max(0.f, dot(normal, H));
//    float nDotV = max(0.f, dot(normal, E));
//    float lDotH = max(0.f, dot(L, H));
//    float att = shade_factor;
//
//    if(light.type > 0)
//    {
//        // distance^2
//        float dist2 = dot(lightDir, lightDir);
//        float v = dist2 / (light.radius * light.radius);
//        v = clamp(1.f - v * v, 0.f, 1.f);
//        att *= v * v / (1.f + dist2 * light.quadraticAttenuation);
//
//        if(light.type > 1)
//        {
//            float spot_effect = dot(normalize(light.direction), -L);
//            att *= spot_effect < light.spotCosCutoff ? 0 : 1;
//            spot_effect = pow(spot_effect, light.spotExponent);
//            att *= spot_effect;
//        }
//    }
//
//    // brdf term
//    const vec3 dielectricF0 = vec3(0.04);
//    vec3 f0 = mix(dielectricF0, base_color.rgb, metalness);
//    vec3 F = F_schlick(f0, lDotH);
//    float D = D_GGX(nDotH, roughness);
//    float Vis = Vis_schlick(nDotL, nDotV, roughness);
//    vec3 Ir = light.diffuse.rgb * light.intensity;
//    vec3 diffuse = BRDF_Lambertian(base_color.rgb, metalness);
//    vec3 specular = F * D * Vis;
//    return vec4((diffuse + specular) * nDotL * Ir * att, 1.0);
//}
