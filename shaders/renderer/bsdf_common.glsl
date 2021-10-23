#define PI 3.1415926535897932384626433832795
#define ONE_OVER_PI 0.31830988618379067153776752674503

//! simple struct to group results from a bsdf-sample
struct bsdf_sample_t
{
    // reflect/refract direction
    vec3 direction;

    //
    vec3 F;

    // probability density
    float pdf;

    // sample passed through a surface
    bool transmission;
};

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

/*
 * Calculates local coordinate frame for a given normal
 */
mat3 local_frame(in vec3 normal)
{
//    vec3 up = abs(normal.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
//    vec3 tangentX = normalize(cross(normal, up));
//    vec3 tangentY = cross(normal, tangentX);
    float len2 = dot(normal.xy, normal.xy);
    vec3 tangentX = len2 > 0 ? vec3(-normal.y, normal.x, 0) / sqrt(len2) : vec3(1, 0, 0);
    vec3 tangentY = cross(normal, tangentX);
    return mat3(tangentX, tangentY, normal);
}

//! sample a Lambert-distribution
vec3 sample_cosine(vec2 Xi)
{
    float cosTheta = sqrt(max(1.0 - Xi.y, 0.0));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float phi = 2.0 * PI * Xi.x;

    // L
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// sample a GGX-distribution
vec3 sample_GGX(vec2 Xi, float roughness)
{
    float a = max(0.001, roughness);

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
    float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));

    // H
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// sample a 'GGX-distribution of visible normals' (from Eric Heitz, 2018)
vec3 sample_GGX_VNDF(vec2 Xi, vec3 V, vec2 roughness)
{
    roughness = max(vec2(0.001), roughness);

    // transform view-direction to hemisphere configuration
    vec3 Vh = normalize(vec3(roughness * V.xy, V.z));

    // orthonormal basis
    mat3 basis = local_frame(Vh);

    // parametrization of projected area
    float phi = 2.0 * PI * Xi.x;
    float r = sqrt(Xi.y);

    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    // reprojection onto hemisphere
    vec3 Nh = t1 * basis[0] + t2 * basis[1] + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    // transforming normal back to ellipsoid configuration
    vec3 Ne = normalize(vec3(roughness * Nh.xy, max(0.0, Nh.z)));
    return Ne;
}

vec3 sample_GTR1(vec2 Xi, float roughness)
{
    float a = max(0.001, roughness);
    float a2 = a * a;

    float phi = Xi.x * 2 * PI;
    float cosTheta = sqrt((1.0 - pow(a2, 1.0 - Xi.y)) / (1.0 - a2));
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);

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

float DielectricFresnel(float cos_theta_i, float eta)
{
    float sinThetaTSq = eta * eta * (1.0f - cos_theta_i * cos_theta_i);

    // Total internal reflection
    if (sinThetaTSq > 1.0)
    return 1.0;

    float cos_theta_t = sqrt(max(1.0 - sinThetaTSq, 0.0));

    float rs = (eta * cos_theta_t - cos_theta_i) / (eta * cos_theta_t + cos_theta_i);
    float rp = (eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t);

    return 0.5f * (rs * rs + rp * rp);
}

/*
 * Generalized-Trowbridge-Reitz (D)
 */
float GTR1(float NDotH, float a)
{
    if (a >= 1.0)
    return (1.0 / PI);

    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;

    return (a2 - 1.0) / (PI * log(a2) * t);
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

/*
 * Power heuristic often reduces variance even further for multiple importance sampling
 * Chapter 13.10.1 of pbrbook
 */
float powerHeuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

vec3 transmittance(vec3 attenuation_color, float attenuation_distance, float distance)
{
    return exp(log(attenuation_color) / attenuation_distance * distance);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
