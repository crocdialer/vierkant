module media_density;

import ray_common;
import "../utils/random.slang";
import "../utils/simplex.slang";
import "../utils/transform.slang";

namespace ray
{

/**
 * @brief   master switch for heterogeneous media, read by every site that would pull the
 *          density-field into the megakernel.
 *
 * false compiles out both the grid-density distance-sampling in media_segment and the ratio-tracked
 * transmittance below. that has to be compile-time: the helpers inline into closest_hit and all
 * three NEE call sites, so they raise the register high-water mark for every ray whether or not a
 * runtime gate ever lets them execute.
 *
 * TEMPORARY. a scene can hold a heterogeneous cloud and homogeneous fog at once, so this must
 * become the per-material runtime gate (material's density-sampler) once that costs little enough
 * to leave compiled in - which is what replacing the procedural placeholder with a texture buys.
 */
public static const bool MEDIA_HETEROGENEOUS = false;

//! the wip density-field is clamped to [0, 1], so 1 bounds it everywhere. a real grid replaces this
//! with its own majorant, per volume or per macrocell
public static const float MEDIA_MAX_DENSITY = 1.0;

//! safety-net against a pathological sigma_t. the roulette in media_transmittance ends walks long
//! before this, so hitting it means the majorant is wrong, not that the medium is dense
public static const uint MEDIA_MAX_TRACKING_STEPS = 512;

//! transmittance below this plays roulette instead of walking a nearly-opaque path to its end
public static const float MEDIA_TRANSMITTANCE_ROULETTE = 0.05;

//! transform into the entry's unit-aabb space, where the density-field is defined
public utils::transform_t to_aabb_norm(Ptr<entry_t, Access::Read> entry)
{
    float3 aabb_size = float3(entry->aabb.max_x - entry->aabb.min_x, entry->aabb.max_y - entry->aabb.min_y,
                              entry->aabb.max_z - entry->aabb.min_z);
    utils::transform_t ret = entry->inv_transform;

    // a flat mesh has a zero extent; dividing by it poisons every sample downstream with NaN
    ret.scale_x /= max(aabb_size.x, EPS);
    ret.scale_y /= max(aabb_size.y, EPS);
    ret.scale_z /= max(aabb_size.z, EPS);
    return ret;
}

//! a ray restated in the entry's unit-aabb space
public Ray to_media_ray(Ptr<entry_t, Access::Read> entry, Ray ray)
{
    utils::transform_t to_media = to_aabb_norm(entry);
    float scale = length(float3(to_media.scale_x, to_media.scale_y, to_media.scale_z));

    Ray ret = ray;
    ret.origin = utils::apply_transform(to_media, ray.origin);
    ret.direction = scale * utils::apply_rotation(to_media, ray.direction);
    return ret;
}

//! NOTE: wip placeholder for a real density-grid. the single definition of the field, so the
//! distance-sampling in media_segment and the transmittance below cannot drift apart
public float media_density(float3 pos, float time)
{
    // NOTE: using simplex here bloat code-size big-time, increasing occupancy
    return MEDIA_MAX_DENSITY;//clamp(0.5 * (utils::simplex(float4(4 * pos, time * 0.1)) + 1.0), 0.0, MEDIA_MAX_DENSITY);
}

/**
 * @brief   ratio-tracked transmittance through an entry's density-field over [0, dist].
 *
 * every sample is treated as a null collision and weighted by the share of the majorant the real
 * extinction did not use. that is unbiased per channel - the same weighting the distance-sample in
 * media_segment applies, so both handle chromatic sigma_t without any spectral MIS.
 *
 * @param   entry       the volume whose aabb the density-field is defined in
 * @param   ray         worldspace shadow-ray
 * @param   dist        distance to integrate, from ray.origin
 * @param   sigma_t     extinction of the medium at density 1
 * @param   time        animation-time of the density-field
 * @param   rng_state   in/out state for RNG
 */
public float3 media_transmittance(Ptr<entry_t, Access::Read> entry, Ray ray, float dist, float3 sigma_t, float time,
                                  inout uint rng_state)
{
    // bounds every channel, so a single walk estimates all three at once
    float majorant = max3(sigma_t) * MEDIA_MAX_DENSITY;
    if(majorant <= 0.0 || dist <= 0.0) { return float3(1); }

    Ray ray_local = to_media_ray(entry, ray);
    float3 transmittance = float3(1);
    float t = 0;

    for(uint i = 0; i < MEDIA_MAX_TRACKING_STEPS; ++i)
    {
        t -= log(1 - utils::rnd(rng_state)) / majorant;
        if(t >= dist) { break; }

        float density = media_density(ray_local.origin + t * ray_local.direction, time);
        transmittance *= float3(1) - sigma_t * density / majorant;

        // a step-cap on its own would bias transmittance high; roulette ends opaque walks instead
        float q = max3(transmittance);
        if(q < MEDIA_TRANSMITTANCE_ROULETTE)
        {
            if(utils::rnd(rng_state) > q) { return float3(0); }
            transmittance /= q;
        }
    }
    return transmittance;
}

}// namespace ray
