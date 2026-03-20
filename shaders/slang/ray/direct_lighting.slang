module direct_lighting;

import "visibility_test.slang";
import "bsdf_disney.slang";
import "types.slang";
import "ray_common.slang";
import "../utils/random.slang";
import "../utils/constants.slang";
import "../utils/sampling.slang";

namespace ray
{

public struct sunlight_params_t
{
    public float3 color;
    public float intensity;
    public float3 direction;
    public float angular_size;
};

public float3 sample_sun_light(material_t material, sunlight_params_t p, RaytracingAccelerationStructure as,
                               float3 pos, float3 N, float3 V, float eta, inout uint rng_state)
{
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    float3 light_dir = mul(utils::local_frame(p.direction), utils::sample_unit_sphere_cap(Xi, p.angular_size));

    ray::Ray r;
    r.origin = pos + ray::EPS * N;
    r.direction = light_dir;
    r.tmin = 0;
    r.tmax = 10000.0;

    float3 radiance = float3(0);
    if (visibility_test(r, as))
    {
        float pdf = 0.0;
        float cos_theta = abs(dot(N, light_dir));
        float3 F = eval_disney(material, light_dir, N, V, eta, pdf);
        if (pdf <= 0 || isnan(pdf)) { return float3(0); }
        radiance = p.color * p.intensity * F * cos_theta / max(pdf, PDF_EPS);
    }
    return radiance;
}

public float3 sample_sun_light_phase(Ray ray, sunlight_params_t p, RaytracingAccelerationStructure as, inout uint rng_state)
{
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    float3 light_dir = mul(utils::local_frame(p.direction), utils::sample_unit_sphere_cap(Xi, p.angular_size));

    Ray light_ray;
    light_ray.origin = ray.origin;
    light_ray.direction = light_dir;
    light_ray.tmin = 0;
    light_ray.tmax = 10000.0;
    return visibility_test(light_ray, as) ? p.color * p.intensity : float3(0);
}

} // namespace ray
