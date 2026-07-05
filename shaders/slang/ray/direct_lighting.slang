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

//! groups a sampled incident direction with unshadowed radiance
public struct light_sample_t
{
    //! normalized direction from shading point toward the light
    public float3 direction;

    //! distance to the light, used as visibility-ray tmax
    public float dist;

    //! solid-angle pdf (1.0 for delta lights)
    public float pdf;

    //! unshadowed incident radiance (includes range-window/falloff/spot attenuation)
    public float3 radiance;
};

//! sample an incident direction and unshadowed radiance for one light
public light_sample_t sample_light(light_t l, float3 P, float2 Xi)
{
    light_sample_t ret;
    ret.pdf = 1.0;
    ret.radiance = l.color * l.intensity;

    if(l.type == LightType::Directional)
    {
        // l.direction stores the propagation direction (glTF convention)
        ret.direction = -l.direction;
        ret.dist = DEFAULT_TMAX;

        // angular_size > 0 marks a sun-style disc-light: sample a direction in its cone
        if(l.angular_size > 0.0)
        {
            ret.direction = mul(utils::sample_unit_sphere_cap(Xi, l.angular_size), utils::local_frame(ret.direction));
            ret.pdf = 1.0 / (2.0 * utils::PI * (1.0 - cos(l.angular_size)));
        }
    }
    else if(l.type == LightType::Sphere)
    {
        // exact solid-angle sampling (Shirley): the sphere subtends a cone with sin(sigma) = r/d
        float3 to_center = l.position - P;
        float d2 = dot(to_center, to_center);
        float r2 = l.size_x * l.size_x;

        if(d2 <= r2)
        {
            // shading point inside the sphere: no valid sample, return black (nee_* skips the shadow ray)
            ret.radiance = float3(0);
            ret.direction = float3(0, 0, 1);
            ret.dist = EPS;
        }
        else
        {
            float sin2_sigma = r2 / d2;
            float cos_sigma = sqrt(1.0 - sin2_sigma);
            ret.direction = mul(utils::sample_unit_sphere_cap(Xi, asin(sqrt(sin2_sigma))),
                                utils::local_frame(to_center * rsqrt(d2)));

            // uniform-cone pdf, cancellation-free form of 1 / (2pi * (1 - cos_sigma))
            ret.pdf = (1.0 + cos_sigma) / (2.0 * utils::PI * sin2_sigma);

            // visibility-ray tmax: distance to sphere-entry along the sampled direction
            float b = dot(ret.direction, to_center);
            ret.dist = b - sqrt(max(b * b - d2 + r2, 0.0));
        }
    }
    else if(l.type == LightType::Rect || l.type == LightType::Disk || l.type == LightType::Tube)
    {
        // uniform-area sample Q on the light surface with emit-normal n, area-pdf converted to solid-angle below.
        // rect/disk are one-sided and emit along l.direction (glTF propagation convention)
        float3 Q, n;
        if(l.type == LightType::Rect)
        {
            float3 bitangent = cross(l.direction, l.tangent);
            Q = l.position + (2.0 * Xi.x - 1.0) * l.size_x * l.tangent + (2.0 * Xi.y - 1.0) * l.size_y * bitangent;
            n = l.direction;
            ret.pdf = 1.0 / (4.0 * l.size_x * l.size_y);
        }
        else if(l.type == LightType::Disk)
        {
            // disks are rotation-symmetric: any fixed frame around the normal works
            Q = l.position + mul(float3(l.size_x * utils::sample_unit_disc(Xi), 0.0), utils::local_frame(l.direction));
            n = l.direction;
            ret.pdf = 1.0 / (utils::PI * l.size_x * l.size_x);
        }
        else// Tube: open cylinder mantle around the axis (l.direction), emitting radially
        {
            float phi = 2.0 * utils::PI * Xi.x;
            n = mul(float3(cos(phi), sin(phi), 0.0), utils::local_frame(l.direction));
            Q = l.position + (2.0 * Xi.y - 1.0) * l.size_y * l.direction + l.size_x * n;
            ret.pdf = 1.0 / (4.0 * utils::PI * l.size_x * l.size_y);
        }

        float3 to_q = Q - P;
        float d2 = dot(to_q, to_q);
        ret.dist = max(sqrt(d2), EPS);
        ret.direction = to_q / ret.dist;

        // backfacing surface elements yield a black sample (nee_* skips the shadow ray)
        float cos_light = dot(n, -ret.direction);
        ret.pdf *= d2 / max(cos_light, EPS);
        if(cos_light <= 0.0) { ret.radiance = float3(0); }
    }
    else// Omni / Spot
    {
        float3 to_light = l.position - P;
        ret.dist = max(length(to_light), EPS);
        ret.direction = to_light / ret.dist;

        // inverse-square falloff with range-window (glTF punctual, range = inf -> pure 1/d^2)
        float w = ret.dist / l.range;
        w *= w;
        float attenuation = clamp(1.0 - w * w, 0.0, 1.0) / (ret.dist * ret.dist);

        if(l.type == LightType::Spot)
        {
            float cd = dot(l.direction, -ret.direction);
            float angular_attenuation = clamp(cd * l.spot_angle_scale + l.spot_angle_offset, 0.0, 1.0);
            attenuation *= angular_attenuation * angular_attenuation;
        }
        ret.radiance *= attenuation;
    }
    return ret;
}

//! next-event estimation at a surface: uniformly pick one light, sample it, trace a visibility ray.
//! requires num_lights > 0
public float3 nee_surface(material_t material, Ptr<light_t, Access.Read> lights, uint num_lights,
                          RaytracingAccelerationStructure as, float3 pos, float3 N, float3 V, float eta,
                          inout uint rng_state)
{
    // uniform 1/N light-pick (single light: keep rng-stream untouched)
    uint index = num_lights > 1 ? min(uint(utils::rnd(rng_state) * num_lights), num_lights - 1) : 0;
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    light_sample_t ls = sample_light(lights[index], pos, Xi);

    // skip the shadow ray for black samples (beyond range, inside a sphere-light)
    if(!any(ls.radiance > 0.0)) { return float3(0); }

    ray::Ray r;
    r.origin = pos + ray::EPS * N;
    r.direction = ls.direction;
    r.tmin = 0;
    r.tmax = ls.dist;

    float3 radiance = float3(0);
    if(visibility_test(r, as))
    {
        // bsdf-pdf is unused until MIS enters the picture (area/env lights)
        float bsdf_pdf = 0.0;
        float cos_theta = max(dot(N, ls.direction), 0.0);
        float3 F = eval_disney(material, ls.direction, N, V, eta, bsdf_pdf);

        // divide by pick-pdf (1/num_lights) and solid-angle pdf
        radiance = float(num_lights) * ls.radiance * F * cos_theta / ls.pdf;
    }
    return radiance;
}

//! next-event estimation at a volume-scattering event, using the phase function.
//! requires num_lights > 0
public float3 nee_volume(Ray ray, Ptr<light_t, Access.Read> lights, uint num_lights,
                         RaytracingAccelerationStructure as, float phase_g, inout uint rng_state)
{
    // uniform 1/N light-pick (single light: keep rng-stream untouched)
    uint index = num_lights > 1 ? min(uint(utils::rnd(rng_state) * num_lights), num_lights - 1) : 0;
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    light_sample_t ls = sample_light(lights[index], ray.origin, Xi);

    // skip the shadow ray for black samples (beyond range, inside a sphere-light)
    if(!any(ls.radiance > 0.0)) { return float3(0); }

    // shadow ray from inside volume: cull back faces from enclosing mesh
    RayDesc shadow_desc;
    shadow_desc.Origin = ray.origin;
    shadow_desc.Direction = ls.direction;
    shadow_desc.TMin = EPS;
    shadow_desc.TMax = ls.dist;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER |
             RAY_FLAG_CULL_BACK_FACING_TRIANGLES> shadow_query;
    shadow_query.TraceRayInline(as, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                                0xFF, shadow_desc);
    shadow_query.Proceed();
    if(shadow_query.CommittedStatus() != COMMITTED_NOTHING) { return float3(0); }

    float phase = utils::phase_hg(dot(ray.direction, ls.direction), phase_g);

    // divide by pick-pdf (1/num_lights) and solid-angle pdf
    return float(num_lights) * ls.radiance * phase / ls.pdf;
}

}// namespace ray
