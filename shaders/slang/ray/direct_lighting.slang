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

//! projector-cookie: radiance-multiplier for one sampled direction, white when the light carries no cookie.
//! Spot projects through its cone (gobo/slide), Omni maps the full sphere in lat-long (photometric profile).
//! delta lights only - a pure multiplier, invisible to pdf/MIS
float3 cookie_factor(light_t l, float3 dir_to_light, Sampler2D textures[])
{
    if(l.texture_index == 0) { return float3(1); }

    // direction from the light toward the shading point, expressed in the light-frame
    float3 D = -dir_to_light;
    float3 p = float3(dot(D, l.tangent), dot(D, cross(l.direction, l.tangent)), dot(D, l.direction));
    float2 uv;

    if(l.type == LightType::Spot)
    {
        // behind the projector
        if(p.z <= 0.0) { return float3(0); }

        // uv_scale carries 1/tan(outer_cone_angle): the cookie-square circumscribes the cone,
        // its corners are cut by the cone's angular attenuation
        uv = 0.5 + 0.5 * p.xy * l.uv_scale / p.z;
        if(any(uv != saturate(uv))) { return float3(0); }
    }
    else
    {
        // lat-long, covering the full sphere: no guards needed
        uv = float2(atan2(p.y, p.x) * 0.5 * utils::ONE_OVER_PI + 0.5, acos(clamp(p.z, -1.0, 1.0)) * utils::ONE_OVER_PI);
    }

    // lod 0: NEE takes one sample per light per bounce, accumulation averages the aliasing away
    return textures[NonUniformResourceIndex(l.texture_index)].SampleLevel(uv, 0).rgb;
}

//! sample an incident direction and unshadowed radiance for one light
public light_sample_t sample_light(light_t l, float3 P, float2 Xi, Sampler2D textures[])
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
        ret.radiance *= attenuation * cookie_factor(l, ret.direction, textures);
    }
    return ret;
}

//! overflow-safe power-heuristic weight (beta = 2): p_a^2 / (p_a^2 + p_b^2)
public float power_heuristic(float p_a, float p_b)
{
    float r = p_b / p_a;
    return 1.0 / (1.0 + r * r);
}

//! whether a light's radiance carries the unbounded 1/d^2 punctual falloff
public bool has_punctual_falloff(light_t l) { return l.type == LightType::Omni || l.type == LightType::Spot; }

//! equiangular distance-sampling along a ray-segment toward a point-light (Kulla/Fajardo 2012):
//! a density proportional to 1/d^2, cancelling the punctual falloff exactly
public struct equiangular_t
{
    //! projection of the light onto the ray-axis
    public float t_center;

    //! perpendicular distance from the light to the ray-axis, floored to keep the density finite
    public float dist_perp;

    //! angles subtended at the segment-ends, the interval sampled uniformly
    public float theta_a, theta_b;

    //! dist_perp / (theta_b - theta_a). factored out so the density needs no epsilon-guard:
    //! a pdf-scale one would clamp it right where it peaks, near the light
    public float norm;

    //! segment-length, the sampled distance is clamped against it
    public float seg_len;

    //! false for a degenerate segment (zero length or zero subtended angle)
    public bool valid;
};

public equiangular_t equiangular_setup(Ray ray, float seg_len, float3 light_pos)
{
    equiangular_t ret;
    float3 to_light = light_pos - ray.origin;
    ret.t_center = dot(to_light, ray.direction);
    ret.dist_perp = max(sqrt(max(dot(to_light, to_light) - ret.t_center * ret.t_center, 0.0)), EPS);
    ret.theta_a = atan2(-ret.t_center, ret.dist_perp);
    ret.theta_b = atan2(seg_len - ret.t_center, ret.dist_perp);
    ret.seg_len = seg_len;

    float delta_theta = ret.theta_b - ret.theta_a;
    ret.valid = seg_len > 0.0 && delta_theta > 0.0;
    ret.norm = ret.valid ? ret.dist_perp / delta_theta : 0.0;
    return ret;
}

//! density at t. shares the struct with equiangular_sample(), so the floored 'dist_perp' stays
//! consistent between sampling and pdf and introduces no bias
public float equiangular_pdf(equiangular_t eq, float t)
{
    float d = t - eq.t_center;
    return eq.norm / (eq.dist_perp * eq.dist_perp + d * d);
}

//! sample a distance in [0, seg_len], writing out its density
public float equiangular_sample(equiangular_t eq, float Xi, out float pdf)
{
    float t = clamp(eq.t_center + eq.dist_perp * tan(lerp(eq.theta_a, eq.theta_b, Xi)), 0.0, eq.seg_len);
    pdf = equiangular_pdf(eq, t);
    return t;
}

//! whether bsdf/phase-sampled rays can hit this light: sun-discs via the miss shaders,
//! analytic light bodies via the raygen ghost-light pass
public bool bsdf_reachable(light_t l)
{
    return (l.type == LightType::Directional && l.angular_size > 0.0) || l.type == LightType::Rect ||
           l.type == LightType::Sphere || l.type == LightType::Tube || l.type == LightType::Disk;
}

//! closed-form ray-intersection with an analytic light body
public struct light_hit_t
{
    //! hit distance, <= 0 marks a miss
    public float t;

    //! cosine at the light surface (facing the ray origin)
    public float cos_light;
};

//! intersect a ray with an analytic light body. front-facing surfaces only: back sides are
//! transparent, consistent with the one-sided black NEE samples. non-area types never hit
public light_hit_t intersect_light(light_t l, float3 P, float3 dir)
{
    light_hit_t ret;
    ret.t = 0.0;
    ret.cos_light = 0.0;

    switch(l.type)
    {
        case LightType::Sphere:
        {
            float3 to_center = l.position - P;
            float d2 = dot(to_center, to_center);
            float r2 = l.size_x * l.size_x;

            // inside the sphere: no hit (matches the black NEE sample)
            if(d2 <= r2) { break; }
            float b = dot(dir, to_center);
            float disc = b * b - d2 + r2;
            if(disc <= 0.0) { break; }
            float t = b - sqrt(disc);
            if(t <= 0.0) { break; }
            ret.t = t;
            ret.cos_light = dot((P + t * dir - l.position) / l.size_x, -dir);
            break;
        }
        case LightType::Rect:
        case LightType::Disk:
        {
            float cos_light = dot(l.direction, -dir);
            if(cos_light <= 0.0) { break; }
            float t = dot(l.position - P, l.direction) / -cos_light;
            if(t <= 0.0) { break; }

            float3 q = P + t * dir - l.position;
            if(l.type == LightType::Rect)
            {
                float3 bitangent = cross(l.direction, l.tangent);
                if(abs(dot(q, l.tangent)) > l.size_x || abs(dot(q, bitangent)) > l.size_y) { break; }
            }
            else if(dot(q, q) > l.size_x * l.size_x) { break; }
            ret.t = t;
            ret.cos_light = cos_light;
            break;
        }
        case LightType::Tube:
        {
            // nearest mantle intersection (quadratic in the plane perpendicular to the axis)
            float3 o = P - l.position;
            float3 d_perp = dir - l.direction * dot(dir, l.direction);
            float3 o_perp = o - l.direction * dot(o, l.direction);
            float A = dot(d_perp, d_perp);
            float B = dot(o_perp, d_perp);
            float C = dot(o_perp, o_perp) - l.size_x * l.size_x;
            float disc = B * B - A * C;

            // parallel to the axis, miss, or P inside the mantle radius (backfacing hit)
            if(A <= EPS || disc <= 0.0 || C <= 0.0) { break; }
            float t = (-B - sqrt(disc)) / A;
            if(t <= 0.0 || abs(dot(o + t * dir, l.direction)) > l.size_y) { break; }

            // radial unit-normal at the hit
            float cos_light = dot((o_perp + t * d_perp) / l.size_x, -dir);
            if(cos_light <= 0.0) { break; }
            ret.t = t;
            ret.cos_light = cos_light;
            break;
        }
        default: break;
    }
    return ret;
}

//! NEE-side MIS weight against a bsdf/phase competitor-pdf, respecting a forced estimator-mode
public float mis_weight_nee(MisMode mode, float p_nee, float p_bsdf)
{
    if(mode == MisMode::NeeOnly) { return 1.0; }
    if(mode == MisMode::BsdfOnly) { return 0.0; }
    return power_heuristic(p_nee, p_bsdf);
}

//! hit-side MIS weight for a bsdf/phase ray that reached a light (prev_bsdf_pdf <= 0: no NEE competitor -> full)
public float mis_weight_hit(MisMode mode, float prev_bsdf_pdf, float p_nee)
{
    if(mode == MisMode::NeeOnly) { return 0.0; }
    if(mode == MisMode::BsdfOnly || prev_bsdf_pdf <= 0.0) { return 1.0; }
    return power_heuristic(prev_bsdf_pdf, p_nee);
}

//! solid-angle pdf of sample_light() generating direction `dir` from P — the hit-side density for MIS.
//! must mirror the sampling strategies in sample_light() exactly
public float light_pdf(light_t l, float3 P, float3 dir)
{
    switch(l.type)
    {
        case LightType::Directional:
        {
            // sun-disc: constant over the cone; delta lights (angular_size == 0) are never bsdf-reachable
            if(l.angular_size <= 0.0) { return 0.0; }
            float cos_sigma = cos(l.angular_size);
            return dot(dir, -l.direction) < cos_sigma ? 0.0 : 1.0 / (2.0 * utils::PI * (1.0 - cos_sigma));
        }
        case LightType::Sphere:
        {
            // constant over the subtended cone
            float3 to_center = l.position - P;
            float d2 = dot(to_center, to_center);
            float r2 = l.size_x * l.size_x;
            if(d2 <= r2) { return 0.0; }
            float sin2_sigma = r2 / d2;
            float cos_sigma = sqrt(1.0 - sin2_sigma);
            return dot(dir, to_center * rsqrt(d2)) < cos_sigma ? 0.0
                                                               : (1.0 + cos_sigma) / (2.0 * utils::PI * sin2_sigma);
        }
        case LightType::Rect:
        case LightType::Disk:
        case LightType::Tube:
        {
            // uniform-area strategies: convert the area-pdf at the front-facing hit
            light_hit_t hit = intersect_light(l, P, dir);
            if(hit.t <= 0.0) { return 0.0; }
            float area = l.type == LightType::Rect   ? 4.0 * l.size_x * l.size_y
                         : l.type == LightType::Disk ? utils::PI * l.size_x * l.size_x
                                                     : 4.0 * utils::PI * l.size_x * l.size_y;
            return hit.t * hit.t / (area * hit.cos_light);
        }
        default: return 0.0;// punctual delta lights: never bsdf-reachable
    }
}

//! draw a light from the power-weighted alias-table, in O(1) from two uniform draws.
//! single light: keep the rng-stream untouched. mirrors vierkant::sample_light_alias_table()
public uint pick_light(trace_data_t trace_data, inout uint rng_state)
{
    uint num_lights = trace_data.params.num_lights;
    if(num_lights <= 1) { return 0; }

    uint index = min(uint(utils::rnd(rng_state) * num_lights), num_lights - 1);
    let bin = trace_data.light_alias_table[index];
    return utils::rnd(rng_state) < bin.threshold ? index : bin.alias;
}

//! probability of pick_light() returning 'index'. NEE divides by it, MIS multiplies it into the
//! nee-pdf. zero for lights the table never proposes, which gives bsdf-sampling full hit-side weight
public float light_pick_pdf(trace_data_t trace_data, uint index)
{
    return trace_data.light_alias_table[index].prob;
}

//! next-event estimation at a surface: pick one light, sample it, trace a visibility ray.
//! requires trace_data.params.num_lights > 0
public float3 nee_surface(material_t material, trace_data_t trace_data, RaytracingAccelerationStructure as,
                          Sampler2D textures[], float3 pos, float3 N, float3 V, float eta, inout uint rng_state)
{
    uint index = pick_light(trace_data, rng_state);
    float pick_pdf = light_pick_pdf(trace_data, index);
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    light_sample_t ls = sample_light(trace_data.lights[index], pos, Xi, textures);

    // skip the shadow ray for black samples (beyond range, inside a sphere-light)
    if(!any(ls.radiance > 0.0)) { return float3(0); }

    ray::Ray r;
    r.origin = pos + ray::EPS * N;
    r.direction = ls.direction;
    r.tmin = 0;
    r.tmax = ls.dist;

    float3 radiance = float3(0);
    float3 transmittance = transmittance_test(r, as, 0, trace_data, textures, rng_state);
    if(any(transmittance > 0.0))
    {
        float bsdf_pdf = 0.0;
        float cos_theta = max(dot(N, ls.direction), 0.0);
        float3 F = eval_disney(material, ls.direction, N, V, eta, bsdf_pdf);

        // MIS vs. bsdf-sampling for lights that bsdf-rays can hit (weight == 1 for all others)
        float weight = bsdf_reachable(trace_data.lights[index])
                               ? mis_weight_nee(trace_data.params.mis_mode, ls.pdf * pick_pdf, bsdf_pdf)
                               : 1.0;

        // divide by pick-pdf and solid-angle pdf; attenuate by through-glass transmittance
        radiance = transmittance * weight * (1.0 / pick_pdf) * ls.radiance * F * cos_theta / ls.pdf;
    }
    return radiance;
}

//! next-event estimation at a point 'pos' inside a medium, toward the light 'index' (from pick_light).
//! 'dir' is the direction the path travels. the medium-throughput up to 'pos' is *not* included - it
//! differs per distance-sampling strategy, so the caller carries it.
//! requires trace_data.num_lights > 0
public float3 nee_volume(uint index, float3 pos, float3 dir, trace_data_t trace_data,
                         RaytracingAccelerationStructure as, Sampler2D textures[], float phase_g,
                         inout uint rng_state)
{
    float pick_pdf = light_pick_pdf(trace_data, index);
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    light_sample_t ls = sample_light(trace_data.lights[index], pos, Xi, textures);

    // skip the shadow ray for black samples (beyond range, inside a sphere-light)
    if(!any(ls.radiance > 0.0)) { return float3(0); }

    // shadow ray from inside volume: cull back faces from enclosing mesh
    Ray r;
    r.origin = pos;
    r.direction = ls.direction;
    r.tmin = EPS;
    r.tmax = ls.dist;
    float3 transmittance = transmittance_test(r, as, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, trace_data, textures, rng_state);
    if(!any(transmittance > 0.0)) { return float3(0); }

    float phase = utils::phase_hg(dot(dir, ls.direction), phase_g);

    // MIS vs. phase-sampling for lights that phase-rays can hit (weight == 1 for all others)
    float weight = bsdf_reachable(trace_data.lights[index])
                           ? mis_weight_nee(trace_data.params.mis_mode, ls.pdf * pick_pdf, phase)
                           : 1.0;

    // divide by pick-pdf and solid-angle pdf; attenuate by through-glass transmittance
    return transmittance * weight * (1.0 / pick_pdf) * ls.radiance * phase / ls.pdf;
}

}// namespace ray
