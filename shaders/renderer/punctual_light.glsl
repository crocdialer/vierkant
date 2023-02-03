#include "../utils/bsdf.glsl"

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1
#define LIGHT_TYPE_DIRECTIONAL 2

//! definition of a punctual lightsource
struct lightsource_t
{
    vec3 position;
    uint type;
    vec3 color;
    float intensity;
    vec3 direction;
    float range;
    float spot_angle_scale;
    float spot_angle_offset;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
vec3 shade(in lightsource_t light, in vec3 V, in vec3 normal, in vec3 position, in vec4 base_color,
           float roughness, float metalness, float shade_factor)
{
    vec3 light_dir = light.type == LIGHT_TYPE_DIRECTIONAL ? -light.direction : (light.position - position);
    vec3 L = normalize(light_dir);
    vec3 H = normalize(L + V);

    float nDotL = max(0.f, dot(normal, L));
    float nDotH = max(0.f, dot(normal, H));
    float nDotV = max(0.f, dot(normal, V));
    float lDotH = max(0.f, dot(L, H));
    float att = shade_factor;

    if(light.type == LIGHT_TYPE_POINT || light.type == LIGHT_TYPE_SPOT)
    {
        // distance^2
        float dist2 = dot(light_dir, light_dir);
        float v = dist2 / (light.range * light.range);
        v = clamp(1.f - v * v, 0.f, 1.f);
        att *= v * v / (1.f + dist2);

        if(light.type == LIGHT_TYPE_SPOT)
        {
            float cd = dot(light.direction, L);
            float angular_attenuation = clamp(cd * light.spot_angle_scale + light.spot_angle_offset, 0.0, 1.0);
            angular_attenuation *= angular_attenuation;
            att *= angular_attenuation;
        }
    }

    // brdf term
    const vec3 dielectricF0 = vec3(0.04);
    vec3 f0 = mix(dielectricF0, base_color.rgb, metalness);
    vec3 F = F_Schlick(lDotH, f0);
    float D = GTR2(nDotH, roughness);
    float G = GeometrySmith(nDotV, nDotL, roughness);
    vec3 Ir = light.color.rgb * light.intensity;
    vec3 diffuse = mix(base_color.rgb * ONE_OVER_PI, vec3(0), metalness);
    vec3 specular = F * D * G;
    return (diffuse + specular) * nDotL * Ir * att;
}