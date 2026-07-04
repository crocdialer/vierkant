#include <vierkant/media.hpp>

namespace vierkant
{

media_t to_media(const medium_params_t &params)
{
    // mirror shaders/slang/raypipeline.slang: attenuation_color/distance give the total extinction
    // sigma_t; scatter_factor * scatter_color is the multi-scatter albedo (rho_ms), inverted to the
    // single-scatter albedo (rho_ss) the medium needs via the Kulla-Conty (2017) approximation.
    glm::vec3 sigma_t = -glm::log(params.attenuation_color) / params.attenuation_distance;
    glm::vec3 rho_ms = params.scatter_factor * params.scatter_color;
    glm::vec3 kc = 4.09712f + 4.20863f * rho_ms - glm::sqrt(9.59217f + 41.6808f * rho_ms + 17.7126f * rho_ms * rho_ms);
    glm::vec3 rho_ss = 1.f - kc * kc;

    media_t media = {};
    media.sigma_s = rho_ss * sigma_t;
    media.sigma_a = (1.f - rho_ss) * sigma_t;
    media.phase_g = params.phase_asymmetry_g;
    media.ior = params.ior;
    media.emission = params.emission_color * params.emission_intensity;
    return media;
}

}// namespace vierkant
