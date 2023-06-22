//
// Created by crocdialer on 22.06.23.
//

#pragma once

#include <vierkant/math.hpp>

namespace vierkant
{

/**
 * @brief   converts a normalized direction to an octahedral mapping (non-equal area, signed normalized).
 *
 * @param   n   a normalized direction.
 * @return  position in octahedral map in [-1,1] for each component.
 */
inline glm::vec2 normalized_vector_to_octahedral_mapping(glm::vec3 n)
{
    // project sphere onto octahedron (|x|+|y|+|z| = 1) and then onto xy-plane.
    glm::vec2 p = glm::vec2(n.x, n.y) * (1.f / (std::abs(n.x) + std::abs(n.y) + std::abs(n.z)));

    // reflect folds of lower hemisphere over the diagonals.
    if(n.z < 0.0)
    {
        p = glm::vec2((1.0 - std::abs(p.y)) * (p.x >= 0.0 ? 1.0 : -1.0),
                      (1.0 - std::abs(p.x)) * (p.y >= 0.0 ? 1.0 : -1.0));
    }
    return p;
}

/**
 * @brief   converts a point on the octahedral map to a normalized direction (non-equal area, signed normalized).
 *
 * @param   p   position in octahedral map in [-1,1] for each component.
 * @return  a rormalized direction
 */
inline glm::vec3 octahedral_mapping_to_normalized_vector(glm::vec2 p)
{
    glm::vec3 n = glm::vec3(p.x, p.y, 1.0 - std::abs(p.x) - std::abs(p.y));

    // Reflect the folds of the lower hemisphere over the diagonals.
    if(n.z < 0.0)
    {
        n.xy() = glm::vec2((1.0 - std::abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0),
                           (1.0 - std::abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0));
    }
    return normalize(n);
}

/**
 * @brief   unpack two 16-bit snorm values from lo/hi bits of a dword.
 *
 * @param   packed  two 16-bit snorm in low/high bits.
 * @return  two float values in [-1,1].
 */
inline glm::vec2 unpack_snorm_2x16(uint32_t packed)
{
    glm::ivec2 bits = glm::ivec2(packed << 16, packed) >> 16;
    glm::vec2 unpacked = glm::max(glm::vec2(bits) / 32767.f, glm::vec2(-1.f));
    return unpacked;
}

/**
 * @brief   pack two floats into 16-bit snorm values in the lo/hi bits of a dword.
 *
 * @param   v   two provided floats
 * @return  two 16-bit snorm in low/high bits.
 */
inline uint32_t pack_snorm_2x16(glm::vec2 v)
{
    v = any(isnan(v)) ? glm::vec2(0) : glm::clamp(v, glm::vec2(-1.f), glm::vec2(1.f));
    auto iv = glm::ivec2(glm::round(v * 32767.f));
    uint32_t packed = (iv.x & 0x0000ffff) | (iv.y << 16);
    return packed;
}

}// namespace vierkant
