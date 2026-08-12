//
// Created by crocdialer on 22.06.23.
//

#pragma once

#include <vierkant/math.hpp>

#include <cmath>

namespace vierkant
{

/**
 * @brief   converts a normalized direction to an octahedral mapping (non-equal area, signed normalized).
 *
 * @param   n   a normalized direction.
 * @return  position in octahedral map in [-1,1] for each component.
 */
inline glm::vec2 normalized_vector_to_octahedral_mapping(const glm::vec3 &n)
{
    // project sphere onto octahedron (|x|+|y|+|z| = 1) and then onto xy-plane.
    glm::vec2 p = glm::vec2(n.x, n.y) * (1.f / (std::abs(n.x) + std::abs(n.y) + std::abs(n.z)));

    // reflect folds of lower hemisphere over the diagonals.
    if(n.z < 0.f)
    {
        p = glm::vec2((1.f - std::abs(p.y)) * (p.x >= 0.f ? 1.f : -1.f),
                      (1.f - std::abs(p.x)) * (p.y >= 0.f ? 1.f : -1.f));
    }
    return p;
}

/**
 * @brief   converts a point on the octahedral map to a normalized direction (non-equal area, signed normalized).
 *
 * @param   p   position in octahedral map in [-1,1] for each component.
 * @return  a normalized direction
 */
inline glm::vec3 octahedral_mapping_to_normalized_vector(const glm::vec2 &p)
{
    glm::vec3 n = glm::vec3(p.x, p.y, 1.f - std::abs(p.x) - std::abs(p.y));

    // Reflect the folds of the lower hemisphere over the diagonals.
    // NOTE: glm's swizzle is a by-value function under GLM_FORCE_XYZW_ONLY, so 'n.xy() = ...'
    // silently assigns to a temporary - both components must be written explicitly.
    if(n.z < 0.f)
    {
        float x = (1.f - std::abs(n.y)) * (n.x >= 0.f ? 1.f : -1.f);
        float y = (1.f - std::abs(n.x)) * (n.y >= 0.f ? 1.f : -1.f);
        n.x = x;
        n.y = y;
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

/**
 * @brief   unpack two 11-bit snorm values from the low 22 bits of a dword.
 *
 * @param   packed  two 11-bit snorm in bits [10:0] and [21:11].
 * @return  two float values in [-1,1].
 */
inline glm::vec2 unpack_snorm_2x11(uint32_t packed)
{
    glm::ivec2 bits = glm::ivec2(packed << 21, packed << 10) >> 21;
    return glm::max(glm::vec2(bits) / 1023.f, glm::vec2(-1.f));
}

/**
 * @brief   pack two floats into 11-bit snorm values in the low 22 bits of a dword.
 *
 * @param   v   two provided floats
 * @return  two 11-bit snorm in bits [10:0] and [21:11].
 */
inline uint32_t pack_snorm_2x11(glm::vec2 v)
{
    v = any(isnan(v)) ? glm::vec2(0) : glm::clamp(v, glm::vec2(-1.f), glm::vec2(1.f));
    auto iv = glm::ivec2(glm::round(v * 1023.f));
    return (iv.x & 0x7ff) | ((iv.y & 0x7ff) << 11);
}

/**
 * @brief   encode a direction as an 11:11 octahedral mapping, picking the rounding of the two
 *          components that minimizes the resulting angular error.
 *
 *          Rounding each component to nearest is only optimal for independent axes, which these are
 *          not - the octahedral fold couples them. Trying all four floor/ceil combinations cuts the
 *          worst-case error by ~30% (0.117 -> 0.082 degrees) for four extra decodes at bake-time.
 *
 * @attention   not mirrored in octahedral_map.slang, which keeps plain rounding. That is safe - the
 *              choice only affects which of four legal encodings the packer emits, and decoding is
 *              identical either way - but it does mean vertices repacked by mesh_compute (skin and
 *              morph) keep the coarser 0.117 degrees.
 *
 * @param   n   a normalized direction.
 * @return  two 11-bit snorm in bits [10:0] and [21:11].
 */
inline uint32_t pack_octahedral_2x11(const glm::vec3 &n)
{
    constexpr float scale = 1023.f;
    glm::vec2 p = normalized_vector_to_octahedral_mapping(n);
    p = any(isnan(p)) ? glm::vec2(0) : glm::clamp(p, glm::vec2(-1.f), glm::vec2(1.f));
    glm::vec2 base = glm::floor(p * scale);

    uint32_t best = 0;
    float best_dot = -2.f;

    for(uint32_t i = 0; i < 4; ++i)
    {
        auto iv = glm::ivec2(glm::clamp(base + glm::vec2(i & 1u, (i >> 1) & 1u), -scale, scale));
        uint32_t packed = (static_cast<uint32_t>(iv.x) & 0x7ff) | ((static_cast<uint32_t>(iv.y) & 0x7ff) << 11);

        // NOTE: a non-finite 'n' leaves every comparison false -> best stays 0, matching the NaN-guards above
        float d = glm::dot(n, octahedral_mapping_to_normalized_vector(unpack_snorm_2x11(packed)));
        if(d > best_dot)
        {
            best_dot = d;
            best = packed;
        }
    }
    return best;
}

/**
 * @brief   construct a deterministic orthonormal basis for a direction.
 *          Duff et al., "Building an Orthonormal Basis, Revisited" (JCGT 2017).
 *
 * @attention   must stay bit-identical to the shader-side implementation in octahedral_map.slang,
 *              otherwise packed tangent-angles decode against a different reference frame.
 *              Note the basis flips discontinuously across n.z == 0 - safe here because both sides
 *              evaluate 'n' from the same quantized bits and 1 - |p.x| - |p.y| contains no multiply
 *              (no fma-contraction can change the sign).
 *
 * @param   n   a normalized direction.
 * @param   b1  output: first basis-vector, orthogonal to n.
 * @param   b2  output: second basis-vector, orthogonal to n and b1.
 */
inline void reference_basis(const glm::vec3 &n, glm::vec3 &b1, glm::vec3 &b2)
{
    float s = n.z >= 0.f ? 1.f : -1.f;
    float a = -1.f / (s + n.z);
    float b = n.x * n.y * a;
    b1 = glm::vec3(1.f + s * n.x * n.x * a, s * b, -s * n.x);
    b2 = glm::vec3(b, s + n.y * n.y * a, -n.y);
}

//! bit-layout of a packed tangent-frame: octahedral normal | tangent-angle | handedness
constexpr uint32_t tangent_frame_oct_bits = 22;
constexpr uint32_t tangent_frame_angle_bits = 9;
constexpr uint32_t tangent_frame_angle_steps = 1u << tangent_frame_angle_bits;
constexpr uint32_t tangent_frame_angle_mask = tangent_frame_angle_steps - 1;
constexpr uint32_t tangent_frame_sign_bit = 1u << 31;

/**
 * @brief   pack an orthonormal tangent-frame into a single dword.
 *
 *          The normal is stored as an 11:11 octahedral mapping, the tangent as its rotation around
 *          that normal relative to reference_basis(), plus one bit of bitangent-handedness.
 *          The angle is measured against the *quantized* normal, so the two quantization-errors
 *          do not compound.
 *
 * @param   n   a normalized normal.
 * @param   t   a tangent, not required to be orthogonal to n (the projection is implicit).
 * @param   w   bitangent handedness, glTF convention: bitangent = cross(n, t) * w.
 * @return  the packed tangent-frame.
 */
inline uint32_t pack_tangent_frame(const glm::vec3 &n, const glm::vec3 &t, float w)
{
    uint32_t oct = pack_octahedral_2x11(n);

    // re-derive the basis from the quantized normal, exactly as the unpack-side will
    glm::vec3 quantized_normal = octahedral_mapping_to_normalized_vector(unpack_snorm_2x11(oct));
    glm::vec3 b1, b2;
    reference_basis(quantized_normal, b1, b2);

    // a tangent parallel to n projects to (0,0) -> atan2(0,0) == 0, an arbitrary but valid frame
    float angle = std::atan2(glm::dot(t, b2), glm::dot(t, b1));
    auto steps = static_cast<uint32_t>(std::lround(angle * (tangent_frame_angle_steps / glm::two_pi<float>())));
    return oct | ((steps & tangent_frame_angle_mask) << tangent_frame_oct_bits) | (w < 0.f ? tangent_frame_sign_bit : 0);
}

/**
 * @brief   unpack a tangent-frame packed by pack_tangent_frame.
 *
 * @param   packed  a packed tangent-frame.
 * @param   n       output: the normal.
 * @param   t       output: the tangent, orthogonal to n.
 * @param   w       output: bitangent handedness, bitangent = cross(n, t) * w.
 */
inline void unpack_tangent_frame(uint32_t packed, glm::vec3 &n, glm::vec3 &t, float &w)
{
    n = octahedral_mapping_to_normalized_vector(unpack_snorm_2x11(packed));
    glm::vec3 b1, b2;
    reference_basis(n, b1, b2);

    float angle = static_cast<float>((packed >> tangent_frame_oct_bits) & tangent_frame_angle_mask) *
                  (glm::two_pi<float>() / tangent_frame_angle_steps);
    t = std::cos(angle) * b1 + std::sin(angle) * b2;
    w = (packed & tangent_frame_sign_bit) ? -1.f : 1.f;
}

}// namespace vierkant
