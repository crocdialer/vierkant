#ifndef RAY_DIRECT_LIGHTING_GLSL
#define RAY_DIRECT_LIGHTING_GLSL

#include "ray_common.glsl"

layout(location = MISS_INDEX_SHADOW) rayPayloadEXT shadow_payload_t payload_shadow;

struct sunlight_params_t
{
    vec3 color;
    float intensity;
    vec3 direction;
    float angular_size;
};

vec3 sample_sun_light(const in material_t material, const in sunlight_params_t p, accelerationStructureEXT as,
                      vec3 pos, vec3 N, vec3 V, float eta, inout uint rng_state)
{
    vec2 Xi = vec2(rnd(rng_state), rnd(rng_state));

    // uniform sample sun-area
    vec3 L_light = local_frame(p.direction) * sample_unit_sphere_cap(Xi, p.angular_size);
    Ray ray = Ray(pos + EPS * N, L_light);
    const uint ray_flags =  gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT;
    float tmin = 0.0;
    float tmax = 10000.0;
    payload_shadow.shadow = true;

    traceRayEXT(as,                 // acceleration structure
                ray_flags,          // rayflags
                0xff,               // cullMask
                0,                  // sbtRecordOffset
                0,                  // sbtRecordStride
                MISS_INDEX_SHADOW,  // missIndex
                ray.origin,         // ray origin
                tmin,               // ray min range
                ray.direction,      // ray direction
                tmax,               // ray max range
                MISS_INDEX_SHADOW); // payload-location

    vec3 radiance = vec3(0);

    // eval light
    if(!payload_shadow.shadow)
    {
        float pdf = 0.0;
        float cos_theta = abs(dot(N, L_light));
        vec3 F = eval_disney(material, L_light, N, V, eta, pdf);
        radiance = p.color * p.intensity * clamp(F * cos_theta / pdf, 0.0, 1.0);
    }
    return radiance;
}

#endif // RAY_DIRECT_LIGHTING_GLSL