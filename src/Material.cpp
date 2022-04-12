//
// Created by crocdialer on 3/12/22.
//

#include <vierkant/Material.hpp>

namespace vierkant
{

std::size_t Material::hash() const
{
    using crocore::hash_combine;

    size_t h = 0;
    hash_combine(h, name);
    hash_combine(h, color);
    hash_combine(h, emission);
    hash_combine(h, metalness);
    hash_combine(h, roughness);
    hash_combine(h, occlusion);
    hash_combine(h, two_sided);
    hash_combine(h, blend_mode);
    hash_combine(h, alpha_cutoff);
    hash_combine(h, transmission);
    hash_combine(h, attenuation_color);
    hash_combine(h, attenuation_distance);
    hash_combine(h, ior);
    hash_combine(h, clearcoat_factor);
    hash_combine(h, clearcoat_roughness_factor);
    hash_combine(h, sheen_color);
    hash_combine(h, sheen_roughness);
    hash_combine(h, iridescence_factor);
    hash_combine(h, iridescence_ior);
    hash_combine(h, iridescence_thickness_range);
    hash_combine(h, depth_test);
    hash_combine(h, depth_write);
    hash_combine(h, cull_mode);

    for(const auto &[type, tex] : textures)
    {
        hash_combine(h, type);
        hash_combine(h, tex);
    }

    hash_combine(h, texture_transform);
    return h;
}

}// namespace vierkant
