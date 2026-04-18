module point_light;

import "../utils/bsdf.slang";
import "../utils/constants.slang";

namespace renderer
{

static const uint LIGHT_TYPE_POINT = 0;
static const uint LIGHT_TYPE_SPOT = 1;
static const uint LIGHT_TYPE_DIRECTIONAL = 2;

public struct lightsource_t
{
    public float3 position;
    public uint type;
    public float3 color;
    public float intensity;
    public float3 direction;
    public float range;
    public float spot_angle_scale;
    public float spot_angle_offset;
};

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

public float3 shade(lightsource_t light, float3 V, float3 normal, float3 position, float4 base_color,
                    float roughness, float metalness, float shade_factor)
{
    float3 light_dir = light.type == LIGHT_TYPE_DIRECTIONAL ? -light.direction : (light.position - position);
    float3 L = normalize(light_dir);
    float3 H = normalize(L + V);

    float nDotL = max(0.f, dot(normal, L));
    float nDotH = max(0.f, dot(normal, H));
    float nDotV = max(0.f, dot(normal, V));
    float lDotH = max(0.f, dot(L, H));
    float att = shade_factor;

    if(light.type == LIGHT_TYPE_POINT || light.type == LIGHT_TYPE_SPOT)
    {
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

    const float3 dielectricF0 = float3(0.04);
    float3 f0 = lerp(dielectricF0, base_color.rgb, metalness);
    float3 F = utils::F_Schlick(lDotH, f0);
    float D = utils::GTR2(nDotH, roughness);
    float G = GeometrySmith(nDotV, nDotL, roughness);
    float3 Ir = light.color.rgb * light.intensity;
    float3 diffuse = lerp(base_color.rgb * utils::ONE_OVER_PI, float3(0), metalness);
    float3 specular = F * D * G;
    return (diffuse + specular) * nDotL * Ir * att;
}

}// namespace renderer
