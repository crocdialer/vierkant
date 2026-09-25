module direct_lighting;

import visibility_test;
import bsdf_disney;
import ray_common;
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

//! spherical-rectangle frame for one shading point, the setup for uniform solid-angle sampling.
//! mirrors vierkant::spherical_rect_setup() / sample_spherical_rect() in punctual_light.hpp
public struct spherical_rect_t
{
    //! the shading point the rect is seen from
    public float3 origin;

    //! rect-local frame: x/y span the rect, z is its emitting normal
    public float3 x, y, z;

    //! rect-extents relative to 'origin', in that frame
    public float x0, x1, y0, y1, z0;

    //! edge-normal z-components and the azimuthal offset, carried from setup into sampling
    public float b0, b1, k;

    //! subtended solid angle. the sampling pdf is its reciprocal
    public float solid_angle;

    //! false when the point sees the rect's back, lies in its plane, or subtends too little to sample
    public bool valid;
};

//! smallest subtended solid angle sampled in solid angle; below it, area-sampling takes over.
//! the test is applied to the distance-based estimate, never to the computed angle - see below
public static const float SPHERICAL_RECT_MIN_SOLID_ANGLE = 1.e-3;

//! build the spherical-rectangle frame for a Rect light seen from 'P'
public spherical_rect_t spherical_rect_setup(light_t l, float3 P)
{
    spherical_rect_t ret;
    ret.valid = false;
    ret.origin = P;
    ret.x0 = ret.x1 = ret.y0 = ret.y1 = 0.0;
    ret.b0 = ret.b1 = ret.k = ret.solid_angle = 0.0;

    // 'direction' and 'tangent' come from one rotation, so the frame is orthonormal and z == cross(x, y)
    ret.x = l.tangent;
    ret.y = cross(l.direction, l.tangent);
    ret.z = l.direction;

    // corner-origin of the rect; size_x/size_y are half-extents
    float3 d = l.position - l.size_x * ret.x - l.size_y * ret.y - P;
    ret.z0 = dot(d, ret.z);

    // the rect emits along 'direction' only: z0 >= 0 is its back side or its own plane
    if(!(ret.z0 < 0.0)) { return ret; }

    // decide on a distance-based estimate of the subtended angle, not on the computed one. the four
    // edge-angles below cancel catastrophically for a distant rect and their sum floors at ~2.4e-7,
    // so a threshold on the result can never fire. area/d^2 tracks the true angle to ~0.2% and is
    // computed from raw dot-products, so it stays meaningful exactly where the other one stops
    float3 to_center = l.position - P;
    float approx_solid_angle = 4.0 * l.size_x * l.size_y / max(dot(to_center, to_center), 1.e-12);
    if(approx_solid_angle < SPHERICAL_RECT_MIN_SOLID_ANGLE) { return ret; }

    ret.x0 = dot(d, ret.x);
    ret.y0 = dot(d, ret.y);
    ret.x1 = ret.x0 + 2.0 * l.size_x;
    ret.y1 = ret.y0 + 2.0 * l.size_y;

    // inner angles of the spherical quad, from the four edge-plane normals
    float3 v00 = float3(ret.x0, ret.y0, ret.z0);
    float3 v01 = float3(ret.x0, ret.y1, ret.z0);
    float3 v10 = float3(ret.x1, ret.y0, ret.z0);
    float3 v11 = float3(ret.x1, ret.y1, ret.z0);

    float3 n0 = normalize(cross(v00, v10));
    float3 n1 = normalize(cross(v10, v11));
    float3 n2 = normalize(cross(v11, v01));
    float3 n3 = normalize(cross(v01, v00));

    float g0 = acos(clamp(-dot(n0, n1), -1.0, 1.0));
    float g1 = acos(clamp(-dot(n1, n2), -1.0, 1.0));
    float g2 = acos(clamp(-dot(n2, n3), -1.0, 1.0));
    float g3 = acos(clamp(-dot(n3, n0), -1.0, 1.0));

    ret.b0 = n0.z;
    ret.b1 = n2.z;
    ret.k = 2.0 * utils::PI - g2 - g3;
    ret.solid_angle = g0 + g1 - ret.k;
    ret.valid = ret.solid_angle > 0.0;
    return ret;
}

//! sample a point on the rect, uniform in solid angle as seen from 'sr.origin'. requires sr.valid
public float3 sample_spherical_rect(spherical_rect_t sr, float2 Xi)
{
    // azimuthal angle -> the x-coordinate of the sampled direction
    float au = Xi.x * sr.solid_angle + sr.k;
    float sin_au = sin(au);

    // the limit of a vanishing sine is |fu| -> inf, which drives cu to 0; clamping reaches it without a NaN
    sin_au = abs(sin_au) < 1.e-10 ? (sin_au < 0.0 ? -1.e-10 : 1.e-10) : sin_au;
    float fu = (cos(au) * sr.b0 - sr.b1) / sin_au;

    float cu = (fu >= 0.0 ? 1.0 : -1.0) / sqrt(fu * fu + sr.b0 * sr.b0);
    cu = clamp(cu, -1.0, 1.0);

    float xu = -(cu * sr.z0) / max(sqrt(1.0 - cu * cu), 1.e-10);
    xu = clamp(xu, sr.x0, sr.x1);

    // marginal along y at xu
    float d2 = xu * xu + sr.z0 * sr.z0;
    float h0 = sr.y0 / sqrt(d2 + sr.y0 * sr.y0);
    float h1 = sr.y1 / sqrt(d2 + sr.y1 * sr.y1);
    float hv = h0 + Xi.y * (h1 - h0), hv2 = hv * hv;
    float yv = hv2 < 1.0 - 1.e-6 ? hv * sqrt(d2 / (1.0 - hv2)) : sr.y1;

    return sr.origin + xu * sr.x + yv * sr.y + sr.z0 * sr.z;
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
    else if(l.type == LightType::Rect)
    {
        // uniform in solid angle: the pdf is constant over the rect, which matches a near-flat
        // integrand far better than an area-sample's d^2/cos. one-sided, emitting along l.direction
        spherical_rect_t sr = spherical_rect_setup(l, P);

        if(!(sr.z0 < 0.0))
        {
            // back side or the rect's own plane: no valid sample, black (nee_* skips the shadow ray)
            ret.radiance = float3(0);
            ret.direction = float3(0, 0, 1);
            ret.dist = EPS;
        }
        else if(sr.valid)
        {
            float3 to_q = sample_spherical_rect(sr, Xi) - P;
            ret.dist = max(length(to_q), EPS);
            ret.direction = to_q / ret.dist;
            ret.pdf = 1.0 / sr.solid_angle;
        }
        else
        {
            // subtends too little to sample in solid angle: uniform-area instead. light_pdf() branches
            // on the same predicate, computed by the same setup, so the two cannot disagree
            float3 to_q = l.position + (2.0 * Xi.x - 1.0) * l.size_x * sr.x +
                          (2.0 * Xi.y - 1.0) * l.size_y * sr.y - P;
            float d2 = dot(to_q, to_q);
            ret.dist = max(sqrt(d2), EPS);
            ret.direction = to_q / ret.dist;

            float cos_light = dot(sr.z, -ret.direction);
            ret.pdf = d2 / (4.0 * l.size_x * l.size_y * max(cos_light, EPS));
            if(cos_light <= 0.0) { ret.radiance = float3(0); }
        }
    }
    else if(l.type == LightType::Disk || l.type == LightType::Tube)
    {
        // uniform-area sample Q on the light surface with emit-normal n, area-pdf converted to solid-angle below.
        // a disk is one-sided and emits along l.direction (glTF propagation convention)
        float3 Q, n;
        if(l.type == LightType::Disk)
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
        {
            // mirrors sample_light(): constant over the subtended solid angle, with the same
            // area-sampling fallback, taken on the same predicate from the same setup
            light_hit_t hit = intersect_light(l, P, dir);
            if(hit.t <= 0.0) { return 0.0; }

            spherical_rect_t sr = spherical_rect_setup(l, P);
            if(sr.valid) { return 1.0 / sr.solid_angle; }
            return hit.t * hit.t / (4.0 * l.size_x * l.size_y * hit.cos_light);
        }
        case LightType::Disk:
        case LightType::Tube:
        {
            // uniform-area strategies: convert the area-pdf at the front-facing hit
            light_hit_t hit = intersect_light(l, P, dir);
            if(hit.t <= 0.0) { return 0.0; }
            float area = l.type == LightType::Disk ? utils::PI * l.size_x * l.size_x
                                                   : 4.0 * utils::PI * l.size_x * l.size_y;
            return hit.t * hit.t / (area * hit.cos_light);
        }
        default: return 0.0;// punctual delta lights: never bsdf-reachable
    }
}

//! floor on the NEE-roulette probability. bsdf-sampling picks up a skipped estimate, so low is cheap
public static const float NEE_P_MIN = 0.05;

//! floor for punctual lights: no bsdf-competitor absorbs a skip. 1.0 -> never roulette them
public static const float NEE_P_MIN_PUNCTUAL = 1.0;

//! probability of playing NEE at a vertex. depth 0 always plays
public float nee_probability(trace_data_t trace_data, float3 beta, uint depth, float p_min)
{
    float scale = trace_data.params.nee_beta_scale * depth;
    return scale > 0.0 ? clamp(max3(beta) / scale, p_min, 1.0) : 1.0;
}

//! outcome of the per-vertex NEE roulette, for one already-picked light
public struct nee_roulette_t
{
    //! probability this NEE was played; the estimate divides by it
    public float prob;

    //! probability for bsdf-reachable lights; prev_bsdf_pdf uses this one
    public float reachable_prob;

    //! false when the roulette skipped NEE at this vertex
    public bool play;
};

//! roll the NEE roulette for the picked light 'l', floored by its type
public nee_roulette_t nee_roulette(trace_data_t trace_data, light_t l, float3 beta, uint depth,
                                   inout uint rng_state)
{
    nee_roulette_t ret;
    ret.reachable_prob = nee_probability(trace_data, beta, depth, NEE_P_MIN);
    ret.prob = bsdf_reachable(l) ? ret.reachable_prob
                                 : nee_probability(trace_data, beta, depth, NEE_P_MIN_PUNCTUAL);
    ret.play = ret.prob >= 1.0 || utils::rnd(rng_state) < ret.prob;
    return ret;
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

//! next-event estimation at a surface, toward the light 'index' (from pick_light):
//! sample it, then trace a visibility ray.
//! 'nee_prob' is the probability this NEE was played; the estimate is divided by it.
//! requires trace_data.params.num_lights > 0
public float3 nee_surface(uint index, material_t material, trace_data_t trace_data,
                          RaytracingAccelerationStructure as, Sampler2D textures[], float3 pos, float3 N, float3 V,
                          float eta, float3 media_sigma_t, uint media_entry_index, float nee_prob,
                          inout uint rng_state)
{
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
    float3 transmittance =
            transmittance_test(r, as, 0, trace_data, textures, index, media_sigma_t, media_entry_index, rng_state);
    if(any(transmittance > 0.0))
    {
        float bsdf_pdf = 0.0;
        float cos_theta = max(dot(N, ls.direction), 0.0);
        float3 F = eval_disney(material, ls.direction, N, V, eta, bsdf_pdf);

        // MIS vs. bsdf-sampling for lights that bsdf-rays can hit (weight == 1 for all others)
        float weight = bsdf_reachable(trace_data.lights[index])
                               ? mis_weight_nee(trace_data.params.mis_mode, nee_prob * ls.pdf * pick_pdf, bsdf_pdf)
                               : 1.0;

        // divide by pick-pdf and solid-angle pdf; attenuate by through-glass transmittance
        radiance = transmittance * weight * (1.0 / (nee_prob * pick_pdf)) * ls.radiance * F * cos_theta / ls.pdf;
    }
    return radiance;
}

//! next-event estimation at a point 'pos' inside a medium, toward the light 'index' (from pick_light).
//! 'dir' is the direction the path travels. the medium-throughput up to 'pos' is *not* included - it
//! differs per distance-sampling strategy, so the caller carries it.
//! 'nee_prob' is the probability this NEE was played; the estimate is divided by it.
//! requires trace_data.num_lights > 0
public float3 nee_volume(uint index, float3 pos, float3 dir, trace_data_t trace_data,
                         RaytracingAccelerationStructure as, Sampler2D textures[], float phase_g,
                         float3 media_sigma_t, uint media_entry_index, float nee_prob,
                         inout uint rng_state)
{
    float pick_pdf = light_pick_pdf(trace_data, index);
    float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
    light_sample_t ls = sample_light(trace_data.lights[index], pos, Xi, textures);

    // skip the shadow ray for black samples (beyond range, inside a sphere-light)
    if(!any(ls.radiance > 0.0)) { return float3(0); }

    // no back-face cull: the enclosing mesh's exit-face bounds the medium this ray starts in
    Ray r;
    r.origin = pos;
    r.direction = ls.direction;
    r.tmin = EPS;
    r.tmax = ls.dist;
    float3 transmittance =
            transmittance_test(r, as, 0, trace_data, textures, index, media_sigma_t, media_entry_index, rng_state);
    if(!any(transmittance > 0.0)) { return float3(0); }

    float phase = utils::phase_hg(dot(dir, ls.direction), phase_g);

    // MIS vs. phase-sampling for lights that phase-rays can hit (weight == 1 for all others)
    float weight = bsdf_reachable(trace_data.lights[index])
                           ? mis_weight_nee(trace_data.params.mis_mode, nee_prob * ls.pdf * pick_pdf, phase)
                           : 1.0;

    // divide by pick-pdf and solid-angle pdf; attenuate by through-glass transmittance
    return transmittance * weight * (1.0 / (nee_prob * pick_pdf)) * ls.radiance * phase / ls.pdf;
}

}// namespace ray
