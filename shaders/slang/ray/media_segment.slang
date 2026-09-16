module media_segment;

import ray_common;
import types;
import direct_lighting;
import "../utils/random.slang";
import "../utils/sampling.slang";
import "../utils/simplex.slang";
import "../utils/transform.slang";
import "../utils/phase_function.slang";

namespace ray
{

float channel_avg(float3 v) { return (v.x + v.y + v.z) / 3.0; }

//! transform into the entry's unit-aabb space, where the density-grid is defined
utils::transform_t to_aabb_norm(Ptr<entry_t, Access::Read> entry)
{
    float3 aabb_size = float3(entry->aabb.max_x - entry->aabb.min_x, entry->aabb.max_y - entry->aabb.min_y,
                              entry->aabb.max_z - entry->aabb.min_z);
    utils::transform_t ret = entry->inv_transform;
    ret.scale_x /= aabb_size.x;
    ret.scale_y /= aabb_size.y;
    ret.scale_z /= aabb_size.z;
    return ret;
}

/**
 * @brief   'media_segment' integrates the participating medium a segment travels through.
 *
 * attenuates payload.beta over [0, segment_end], adds volumetric emission and the single-scatter
 * estimates, and on a sampled scatter-event moves payload.ray to the scatter-point and re-aims it
 * through the phase-function. rolls the NEE roulette and writes payload.prev_bsdf_pdf.
 * shared by the geometry- and the light-body closest-hit, so both estimators integrate
 * the same medium.
 *
 * @param   segment_end  distance to the surface or light-body this segment ends on
 * @param   entry        scene-entry of that surface. only the wip grid-density branch reads it,
 *                       so a light-body hit passes null
 * @return  true when the path scattered before reaching 'segment_end'
 */
public bool media_segment(inout payload_t payload, float segment_end, Ptr<entry_t, Access::Read> entry,
                          trace_data_t trace_data, RaytracingAccelerationStructure as, Sampler2D textures[],
                          inout uint rng_state)
{
    bool sample_medium = false;
    float3 sigma_t = payload.media.sigma_s + payload.media.sigma_a;

    // the medium this segment travels through: shadow-rays leaving it are attenuated by it.
    // kept, because payload.media is replaced with the medium behind the surface further down
    const float3 segment_sigma_t = sigma_t;

    // NEE/direct-lighting toward gathered lights[] (sun folded in as directional disc-light)
    bool has_lights = trace_data.params.num_lights > 0;

    if(any(sigma_t > float3(0)))
    {
        float t = 0;
        int channel = int(min(utils::rnd(rng_state) * 3, 2));
        const bool homogenous_medium = true;

        float3 sigma_s = payload.media.sigma_s;

        // throughput entering this segment: the equiangular estimator carries its own transmittance
        float3 seg_beta = payload.beta;

        bool equiangular = false;
        equiangular_t eq = {};

        // one light-pick shared by both single-scatter strategies, so their MIS-weights refer
        // to the same integrand. precedes the roulette, whose floor depends on the light's type
        uint light_index = 0;
        nee_roulette_t roulette = {1.0, 1.0, false};

        if(has_lights)
        {
            light_index = pick_light(trace_data, rng_state);
            roulette =
                    nee_roulette(trace_data, trace_data.lights[light_index], payload.beta, payload.depth, rng_state);
        }
        bool use_direct_lighting = roulette.play;
        float nee_prob = roulette.prob;

        if(use_direct_lighting)
        {
            // punctual lights are never bsdf-reachable, so this never competes with the
            // phase-sampling MIS inside nee_volume
            light_t l = trace_data.lights[light_index];
            equiangular = homogenous_medium && has_punctual_falloff(l);

            // set up before the scatter-branch moves payload.ray.origin off the segment-start
            if(equiangular)
            {
                eq = equiangular_setup(payload.ray, segment_end, l.position);
                equiangular = eq.valid;
            }
        }

        // density of a transmittance-sampled scatter at t
        float dist_pdf = 0.0;

        if(homogenous_medium)
        {
            // emissive medium: analytic emission integral over the full segment,
            // integral(Tr(t) * sigma_a * Le, dt) = sigma_a * Le * (1 - exp(-sigma_t * s)) / sigma_t
            if(any(payload.media.emission > float3(0)))
            {
                float3 tr_int = (float3(1) - exp(-sigma_t * segment_end)) / max(sigma_t, float3(EPS));
                payload.radiance += payload.beta * payload.media.sigma_a * payload.media.emission * tr_int;
            }

            t = min(-log(1 - utils::rnd(rng_state)) / max(sigma_t[channel], EPS), segment_end);
            sample_medium = t < segment_end;

            float3 beam_tr = exp(-sigma_t * t);
            float3 density = sample_medium ? beam_tr * sigma_t : beam_tr;
            float pdf = max(channel_avg(density), PDF_EPS);
            payload.beta *= sample_medium ? (sigma_s * beam_tr / pdf) : (beam_tr / pdf);
            dist_pdf = sample_medium ? pdf : 0.0;
        }
        else
        {
            // NOTE: wip grid-density media with monochromatic attenuation
            const float inv_max_density = 1.0;
            utils::transform_t to_media = to_aabb_norm(entry);

            Ray ray_local = payload.ray;
            float scale = length(float3(to_media.scale_x, to_media.scale_y, to_media.scale_z));
            ray_local.origin = utils::apply_transform(to_media, ray_local.origin);
            ray_local.direction = scale * utils::apply_rotation(to_media, ray_local.direction);

            // event-ratios at a real collision are density-independent (both coefficients scale with density)
            float3 scatter_ratio = sigma_s / max(sigma_t, float3(EPS));

            while(true)
            {
                t -= log(1 - utils::rnd(rng_state)) * inv_max_density / sigma_t[channel];
                if(t >= segment_end) { break; }

                float3 p = ray_local.origin + t * ray_local.direction;
                float density =
                        clamp(0.5 * (utils::simplex(float4(4 * p, trace_data.params.time * 0.1)) + 1.0), 0.0, 1.0);
                if(density * inv_max_density > utils::rnd(rng_state))
                {
                    // real collision: weighted event-split instead of analog absorb-vs-scatter.
                    // emission via collision-estimator (sigma_a/sigma_t * Le), then continue as
                    // scattering with throughput scaled by the single-scattering albedo
                    if(any(payload.media.emission > float3(0)))
                    {
                        payload.radiance += payload.beta * (float3(1) - scatter_ratio) * payload.media.emission;
                    }
                    payload.beta *= scatter_ratio;

                    sample_medium = true;
                    break;
                }
            }
        }

        // second estimate of the same single-scatter integral, MIS-combined with the
        // transmittance-sampled one below. contributes whether or not the distance-sample
        // scattered, which is what keeps thin media from losing the light-shaft entirely
        if(equiangular)
        {
            float eq_pdf;
            float t_eq = equiangular_sample(eq, utils::rnd(rng_state), eq_pdf);
            float3 beam_tr = exp(-sigma_t * t_eq);
            float w = power_heuristic(eq_pdf, max(channel_avg(beam_tr * sigma_t), PDF_EPS));

            float3 pos = payload.ray.origin + payload.ray.direction * t_eq;
            float3 nee = nee_volume(light_index, pos, payload.ray.direction, trace_data, as, textures,
                                    payload.media.phase_g, segment_sigma_t, nee_prob, rng_state);
            payload.radiance += seg_beta * beam_tr * sigma_s * w * nee / eq_pdf;
        }

        if(sample_medium)
        {
            const float g = payload.media.phase_g;
            float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
            payload.ray.origin += payload.ray.direction * t;

            if(use_direct_lighting)
            {
                float w = equiangular ? power_heuristic(dist_pdf, equiangular_pdf(eq, t)) : 1.0;
                payload.radiance += payload.beta * w *
                                    nee_volume(light_index, payload.ray.origin, payload.ray.direction, trace_data, as,
                                               textures, g, segment_sigma_t, nee_prob, rng_state);
            }

            float phase_pdf;
            payload.ray.direction =
                    mul(utils::sample_phase_hg(Xi, g, phase_pdf), utils::local_frame(payload.ray.direction));

            // solid-angle pdf of this bounce for MIS at the next vertex (<= 0: no NEE competitor here)
            payload.prev_bsdf_pdf = has_lights ? phase_pdf / roulette.reachable_prob : 0.0;
            payload.regularize = payload.regularize || phase_pdf < REGULARIZE_PDF_THRESHOLD;
        }
    }

    return sample_medium;
}

}// namespace ray
