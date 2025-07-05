//
// Created by crocdialer on 3/12/22.
//

#include <vierkant/Material.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

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

    for(const auto &[type, tex_data]: m.texture_data)
    {
        hash_combine(h, type);
        hash_combine(h, tex_data.texture_transform);
        hash_combine(h, tex_data.texture_id);
        hash_combine(h, tex_data.sampler_id);
    }
    return h;
}
