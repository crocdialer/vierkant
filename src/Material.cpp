//
// Created by crocdialer on 3/12/22.
//

#include <vierkant/Material.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

bool operator==(const vierkant::material_t &lhs, const vierkant::material_t &rhs)
{
    if(lhs.id != rhs.id) { return false; }
    if(lhs.name != rhs.name) { return false; }
    if(lhs.base_color != rhs.base_color) { return false; }
    if(lhs.emission != rhs.emission) { return false; }
    if(lhs.emissive_strength != rhs.emissive_strength) { return false; }
    if(lhs.roughness != rhs.roughness) { return false; }
    if(lhs.metalness != rhs.metalness) { return false; }
    if(lhs.occlusion != rhs.occlusion) { return false; }
    if(lhs.null_surface != rhs.null_surface) { return false; }
    if(lhs.twosided != rhs.twosided) { return false; }
    if(lhs.ior != rhs.ior) { return false; }
    if(lhs.attenuation_color != rhs.attenuation_color) { return false; }
    if(lhs.transmission != rhs.transmission) { return false; }
    if(lhs.attenuation_distance != rhs.attenuation_distance) { return false; }
    if(lhs.phase_asymmetry_g != rhs.phase_asymmetry_g) { return false; }
    if(lhs.scattering_ratio != rhs.scattering_ratio) { return false; }
    if(lhs.thickness != rhs.thickness) { return false; }
    if(lhs.blend_mode != rhs.blend_mode) { return false; }
    if(lhs.alpha_cutoff != rhs.alpha_cutoff) { return false; }
    if(lhs.specular_factor != rhs.specular_factor) { return false; }
    if(lhs.specular_color != rhs.specular_color) { return false; }
    if(lhs.clearcoat_factor != rhs.clearcoat_factor) { return false; }
    if(lhs.clearcoat_roughness_factor != rhs.clearcoat_roughness_factor) { return false; }
    if(lhs.sheen_color != rhs.sheen_color) { return false; }
    if(lhs.sheen_roughness != rhs.sheen_roughness) { return false; }
    if(lhs.iridescence_factor != rhs.iridescence_factor) { return false; }
    if(lhs.iridescence_ior != rhs.iridescence_ior) { return false; }
    if(lhs.iridescence_thickness_range != rhs.iridescence_thickness_range) { return false; }
    if(lhs.texture_transform != rhs.texture_transform) { return false; }
    if(lhs.textures != rhs.textures) { return false; }
    if(lhs.samplers != rhs.samplers) { return false; }
    return true;
}

}// namespace vierkant

using vierkant::hash_combine;

size_t std::hash<vierkant::material_t>::operator()(vierkant::material_t const &m) const
{
    size_t h = 0;
    hash_combine(h, m.id);
    hash_combine(h, m.name);
    hash_combine(h, m.base_color);
    hash_combine(h, m.emission);
    hash_combine(h, m.emissive_strength);
    hash_combine(h, m.roughness);
    hash_combine(h, m.metalness);
    hash_combine(h, m.occlusion);
    hash_combine(h, m.null_surface);
    hash_combine(h, m.twosided);
    hash_combine(h, m.blend_mode);
    hash_combine(h, m.alpha_cutoff);
    hash_combine(h, m.transmission);
    hash_combine(h, m.phase_asymmetry_g);
    hash_combine(h, m.scattering_ratio);
    hash_combine(h, m.attenuation_color);
    hash_combine(h, m.attenuation_distance);
    hash_combine(h, m.ior);
    hash_combine(h, m.clearcoat_factor);
    hash_combine(h, m.clearcoat_roughness_factor);
    hash_combine(h, m.sheen_color);
    hash_combine(h, m.sheen_roughness);
    hash_combine(h, m.iridescence_factor);
    hash_combine(h, m.iridescence_ior);
    hash_combine(h, m.iridescence_thickness_range);
    hash_combine(h, m.texture_transform);

    for(const auto &[type, tex_id]: m.textures)
    {
        hash_combine(h, type);
        hash_combine(h, tex_id);
    }
    for(const auto &[type, sampler_id]: m.samplers)
    {
        hash_combine(h, type);
        hash_combine(h, sampler_id);
    }
    return h;
}
