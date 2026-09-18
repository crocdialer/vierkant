module media_segment;

import ray_common;
import direct_lighting;
import media_density;
import "../utils/random.slang";
import "../utils/sampling.slang";
import "../utils/transform.slang";
import "../utils/phase_function.slang";
import "../utils/constants.slang";

namespace ray
{

float channel_avg(float3 v) { return (v.x + v.y + v.z) / 3.0; }

/**
 * @brief   'media_segment' integrates the participating medium a segment travels through.
 *
 * attenuates payload.beta over [0, segment_end], adds volumetric emission and the single-scatter
 * estimates, and on a sampled scatter-event moves payload.ray to the scatter-point and re-aims it
 * through the phase-function. rolls the NEE roulette and writes payload.prev_bsdf_pdf.
 * shared by the geometry- and the light-body closest-hit, so both estimators integrate
 * the same medium.
 *
 * @param   payload         current payload
 * @param   segment_end     distance to the surface or light-body this segment ends on
 * @param   trace_data_t    user-provided trace-data, entry-point for everything else
 * @param   as              a raytracing acceleration-structure
 * @param   textures        global texture-array
 * @param   rng_state       in/out state for RNG
 * @return  true when the path scattered before reaching 'segment_end'
 */
public bool media_segment(inout payload_t payload, float segment_end,
                          trace_data_t trace_data, RaytracingAccelerationStructure as, Sampler2D textures[],
                          inout uint rng_state)
{
    bool sample_medium = false;
    float3 sigma_t = payload.media.sigma_s + payload.media.sigma_a;

    // the medium this segment travels through: shadow-rays leaving it are attenuated by it.
    // kept, because payload.media is replaced with the medium behind the surface further down
    const float3 segment_sigma_t = sigma_t;
    const uint segment_entry_index = payload.media.entry_index;

    // NEE/direct-lighting toward gathered lights[] (sun folded in as directional disc-light)
    bool has_lights = trace_data.params.num_lights > 0;

    if(any(sigma_t > float3(0)))
    {
        float t = 0;
        int channel = int(min(utils::rnd(rng_state) * 3, 2));

        // a medium without a scene-entry has no bounds to define a density-grid in, so it stays
        // analytic. camera-seeded media are the case that has none
        const bool homogenous_medium = true;//payload.media.entry_index == utils::MAX_UINT16;

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
            // the volume this medium came from, recorded when the path entered it
            let entry = trace_data.entries + payload.media.entry_index;

            // NOTE: wip grid-density media with monochromatic attenuation. the NEE transmittance in
            // visibility_test ratio-tracks the same field, which is chromatic - only this
            // distance-sample carries the single-channel restriction
            const float inv_max_density = 1.0 / MEDIA_MAX_DENSITY;
            Ray ray_local = to_media_ray(entry, payload.ray);

            // event-ratios at a real collision are density-independent (both coefficients scale with density)
            float3 scatter_ratio = sigma_s / max(sigma_t, float3(EPS));

            while(true)
            {
                // max(): a channel with zero extinction would make the step 0/0 and never advance
                t -= log(1 - utils::rnd(rng_state)) * inv_max_density / max(sigma_t[channel], EPS);
                if(t >= segment_end) { break; }

                float density = media_density(ray_local.origin + t * ray_local.direction,
                                              trace_data.params.time);
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
                                    payload.media.phase_g, segment_sigma_t, segment_entry_index, nee_prob,
                                    rng_state);
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
                                               textures, g, segment_sigma_t, segment_entry_index, nee_prob,
                                               rng_state);
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
