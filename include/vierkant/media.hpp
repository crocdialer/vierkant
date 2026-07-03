//
// participating-media description + conversion, shared by host and path-tracer shader.
//

#pragma once

#include <vierkant/math.hpp>

namespace vierkant
{

//! participating-medium description (matches the shader-side ray::media_t)
struct alignas(16) media_t
{
    glm::vec3 sigma_s = glm::vec3(0.f);
    float ior = 1.f;
    glm::vec3 sigma_a = glm::vec3(0.f);
    float phase_g = 0.f;
};

//! material-facing volume parameters (subset of vierkant::Material), the input the user/geometry
//! provides. converted to a media_t via to_media(). defaults describe a mild absorbing haze.
struct medium_params_t
{
    //! fraction of light transmitted over 'attenuation_distance' (Beer-Lambert), per channel
    glm::vec3 attenuation_color = glm::vec3(0.7f);

    //! distance over which 'attenuation_color' is applied
    float attenuation_distance = 1.f;

    //! overall scattering strength [0, 1] (glTF KHR_materials_scatter scatterFactor)
    float scatter_factor = 0.f;

    //! multi-scatter albedo / scattering tint (glTF multiscatterColorFactor)
    glm::vec3 scatter_color = glm::vec3(1.f);

    //! phase-function asymmetry (forward- vs. back-scattering) [-1, 1]
    float phase_asymmetry_g = 0.f;

    //! index of refraction of the medium
    float ior = 1.f;
};

//! convert material-facing volume-parameters into a media_t, mirroring the path-tracer shader
//! (raypipeline.slang). single host-side helper so inside/outside appearance can't drift.
media_t to_media(const medium_params_t &params);

}// namespace vierkant
