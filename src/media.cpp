#include <algorithm>
#include <vierkant/media.hpp>

namespace vierkant
{

media_t to_media(const medium_params_t &params)
{
    // attenuation_color/distance give the total extinction sigma_t
    // scatter_factor * scatter_color is the multi-scatter albedo (rho_ms), inverted to the
    // single-scatter albedo (rho_ss) via Kulla-Conty (2017) approximation.
    const glm::vec3 sigma_t = -glm::log(params.attenuation_color) / params.attenuation_distance;
    const glm::vec3 rho_ms = params.scatter_factor * params.scatter_color;
    const glm::vec3 kc =
            4.09712f + 4.20863f * rho_ms - glm::sqrt(9.59217f + 41.6808f * rho_ms + 17.7126f * rho_ms * rho_ms);
    const glm::vec3 rho_ss = 1.f - kc * kc;

    media_t media = {};
    media.sigma_s = rho_ss * sigma_t;
    media.sigma_a = (1.f - rho_ss) * sigma_t;
    media.phase_g = std::clamp(params.phase_asymmetry_g, -MAX_PHASE_ASYMMETRY_G, MAX_PHASE_ASYMMETRY_G);
    media.ior = std::clamp(params.ior, 1.f, MAX_IOR);
    media.emission = params.emission_color * params.emission_intensity;
    return media;
}

}// namespace vierkant
