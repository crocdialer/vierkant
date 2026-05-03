module direct_lighting;

import "visibility_test.slang";
import "bsdf_disney.slang";
import "types.slang";
import "ray_common.slang";
import "../utils/random.slang";
import "../utils/constants.slang";
import "../utils/sampling.slang";
import "../utils/phase_function.slang";

namespace ray
{

public struct sunlight_params_t
{
    public float3 color;
    public float intensity;
    public float3 direction;
    public float angular_size;
};

public float3 sample_sun_light(material_t material, sunlight_params_t p, RaytracingAccelerationStructure as, float3 pos,
                               float3 N, float3 V, float eta, inout uint rng_state)
{
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    float3 light_dir = mul(utils::sample_unit_sphere_cap(Xi, p.angular_size), utils::local_frame(p.direction));

    ray::Ray r;
    r.origin = pos + ray::EPS * N;
    r.direction = light_dir;
    r.tmin = 0;
    r.tmax = 10000.0;

    float3 radiance = float3(0);
    if(visibility_test(r, as))
    {
        float unused_pdf = 0.0;
        float cos_theta = max(dot(N, light_dir), 0.0);
        float3 F = eval_disney(material, light_dir, N, V, eta, unused_pdf);
        float pdf_light = 1.0 / (2.0 * utils::PI * (1.0 - cos(p.angular_size)));
        radiance = p.color * p.intensity * F * cos_theta / pdf_light;
    }
    return radiance;
}

public float3 sample_sun_light_phase(Ray ray, sunlight_params_t p, RaytracingAccelerationStructure as,
                                     float phase_g, inout uint rng_state)
{
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    float3 light_dir = mul(utils::sample_unit_sphere_cap(Xi, p.angular_size), utils::local_frame(p.direction));

    Ray light_ray;
    light_ray.origin = ray.origin;
    light_ray.direction = light_dir;
    light_ray.tmin = 0;
    light_ray.tmax = 10000.0;

    if(!visibility_test(light_ray, as)) return float3(0);

    float phase = utils::phase_hg(dot(ray.direction, light_dir), phase_g);
    float pdf_light = 1.0 / (2.0 * utils::PI * (1.0 - cos(p.angular_size)));
    return p.color * p.intensity * phase / pdf_light;
}

}// namespace ray
