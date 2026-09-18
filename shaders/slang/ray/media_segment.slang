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

        // a medium without bounds cannot define a density-grid, so it stays analytic - and with
        // MEDIA_HETEROGENEOUS off, all of them do
        const bool homogenous_medium = !MEDIA_HETEROGENEOUS || payload.media.entry_index == utils::MAX_UINT16;

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

            Ray ray_local = to_media_ray(entry, payload.ray);

            // one achromatic majorant bounds every channel, the same one media_transmittance
            // ratio-tracks against, so both estimators integrate the field the same way
            const float majorant = max3(sigma_t) * MEDIA_MAX_DENSITY;

            // weighted delta tracking: a real collision ends the walk, a null one reweights per
            // channel. analog tracking would carry the extinction in the collision-probability of
            // a single channel and attenuate achromatically
            while(true)
            {
                t -= log(1 - utils::rnd(rng_state)) / majorant;
                if(t >= segment_end) { break; }

                float density = media_density(ray_local.origin + t * ray_local.direction,
                                              trace_data.params.time);
                float3 sigma_t_density = sigma_t * density;

                // channel-average, never 1: a hero-channel probability reaches 1 on the majorant's
                // own channel, leaving no null collision to carry the other channels
                float p_real = channel_avg(sigma_t_density) / majorant;

                if(utils::rnd(rng_state) < p_real)
                {
                    // real collision: emission via collision-estimator (sigma_a * Le), then
                    // continue as scattering. both divided by the density of the event
                    float inv_pdf = 1.0 / max(majorant * p_real, PDF_EPS);

                    if(any(payload.media.emission > float3(0)))
                    {
                        payload.radiance +=
                                payload.beta * payload.media.sigma_a * density * payload.media.emission * inv_pdf;
                    }
                    payload.beta *= sigma_s * density * inv_pdf;

                    sample_medium = true;
                    break;
                }

                // null collision: the per-channel share of the majorant the real extinction left unused
                payload.beta *= (float3(majorant) - sigma_t_density) / max(majorant * (1.0 - p_real), PDF_EPS);
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
