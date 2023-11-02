#ifndef RAY_DIRECT_LIGHTING_GLSL
#define RAY_DIRECT_LIGHTING_GLSL

#include "visibility_test.glsl"
#include "bsdf_disney.glsl"

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
    // uniform sample sun-area
    vec2 Xi = vec2(rnd(rng_state), rnd(rng_state));
    vec3 light_dir = local_frame(p.direction) * sample_unit_sphere_cap(Xi, p.angular_size);
    Ray ray = Ray(pos + EPS * N, light_dir);
    vec3 radiance = vec3(0);

    // eval light
    if(visibility_test(ray, as))
    {
        float pdf = 0.0;
        float cos_theta = abs(dot(N, light_dir));
        vec3 F = eval_disney(material, light_dir, N, V, eta, pdf);
        if(pdf <= 0){ return vec3(0); }
        radiance = p.color * p.intensity * F * cos_theta / (pdf + PDF_EPS);
    }
    return radiance;
}

vec3 sample_sun_light_phase(Ray ray, const in sunlight_params_t p, accelerationStructureEXT as, inout uint rng_state)
{
    // uniform sample sun-area
    vec2 Xi = vec2(rnd(rng_state), rnd(rng_state));
    vec3 light_dir = local_frame(p.direction) * sample_unit_sphere_cap(Xi, p.angular_size);
    Ray light_ray = Ray(ray.origin, light_dir);
    return visibility_test(light_ray, as) ? p.color * p.intensity : vec3(0);
}

#endif // RAY_DIRECT_LIGHTING_GLSL