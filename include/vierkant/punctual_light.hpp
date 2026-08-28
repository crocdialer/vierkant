//
// Created by crocdialer on 4/13/22.
//

#pragma once

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <crocore/NamedUUID.hpp>
#include <vierkant/Material.hpp>
#include <vierkant/math.hpp>
#include <vierkant/object_component.hpp>
#include <vierkant/transform.hpp>

namespace vierkant
{

DEFINE_NAMED_UUID(LightId)

enum class LightType : uint32_t
{
    Omni = 0,
    Spot,
    Directional,

    //! reserved for emissive-triangle lights (later phase)
    Area,

    // analytic area lights
    Rect,
    Sphere,
    Tube,
    Disk
};

//! true for the punctual light-types every renderer can shade (Omni/Spot/Directional)
static inline bool is_punctual(LightType type)
{
    return type == LightType::Omni || type == LightType::Spot || type == LightType::Directional;
}

//! lightsource-asset, owned by an AssetProvider and referenced by LightId
struct lightsource_t
{
    vierkant::LightId id;
    std::string name;

    LightType type = LightType::Omni;
    glm::vec3 color = glm::vec3(1);
    float intensity = 1.f;
    float range = std::numeric_limits<float>::infinity();
    float inner_cone_angle = 0.f;
    float outer_cone_angle = glm::quarter_pi<float>();

    //! area-light extents: x = radius (Sphere/Disk/Tube) or half-width (Rect), y = half-height (Rect) / half-length (Tube)
    glm::vec2 size = glm::vec2(1.f);

    //! optional projector-cookie, modulating emitted radiance. Spot: projected through the cone (gobo/slide),
    //! Omni: lat-long over the full sphere (photometric profile). ignored for all other types
    std::optional<vierkant::texture_data_t> cookie;
};

//! lightsource object-component, referencing a lightsource-asset. position/direction come from the object-transform
struct lightsource_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();
    vierkant::LightId light_id = vierkant::LightId::nil();
};

//! padded buffer-data
struct alignas(16) light_t
{
    glm::vec3 position;
    uint32_t type;
    glm::vec3 color;
    float intensity;
    glm::vec3 direction;
    float range;
    float spot_angle_scale;
    float spot_angle_offset;

    //! >0 on directional lights indicates a sun-style disc-light (apex angle in radians)
    float angular_size;

    //! area-light extent: half-height (Rect) / half-length (Tube)
    float size_y;

    //! area-light in-plane x-axis (Rect)
    glm::vec3 tangent;

    //! area-light extent: radius (Sphere/Disk/Tube) / half-width (Rect)
    float size_x;

    //! projector-cookie uv-scale. Spot: 1/tan(outer_cone_angle), folded in to keep the shader trig-free
    glm::vec2 uv_scale;

    //! index into the renderer's texture-array, 0 marks 'no cookie' (index 0 is a solid-white placeholder)
    uint32_t texture_index;

    //! explicit trailing padding. the shader-side buffer-reference layout derives its array-stride from the
    //! declared members only and does not round up to the struct-alignment, so the pad has to be spelled out
    uint32_t pad;
};
static_assert(sizeof(light_t) == 96, "light_t layout must match shader-side (ray_common.slang)");

static inline light_t convert_light(const vierkant::lightsource_t &light, const vierkant::transform_t &t)
{
    light_t ret = {};
    ret.position = t.translation;
    ret.type = static_cast<uint32_t>(light.type);
    ret.color = light.color;
    ret.intensity = light.intensity;
    ret.direction = t.rotation * glm::vec3(0.f, 0.f, -1.f);
    ret.range = light.range > 0.f ? light.range : std::numeric_limits<float>::infinity();

    ret.spot_angle_scale =
            1.f / std::max(0.001f, std::cos(light.inner_cone_angle) - std::cos(light.outer_cone_angle));
    ret.spot_angle_offset = -std::cos(light.outer_cone_angle) * ret.spot_angle_scale;

    // area-light extents come from the asset, transform-scale is ignored (ill-defined for sphere/tube)
    ret.tangent = t.rotation * glm::vec3(1.f, 0.f, 0.f);
    ret.size_x = light.size.x;
    ret.size_y = light.size.y;

    // projector-cookie: the spot-map divides by tan(outer_cone_angle), pre-inverted here.
    // texture_index stays 0 ('no cookie'), renderers own their texture-array and resolve it.
    ret.uv_scale = glm::vec2(1.f / std::max(1.e-4f, std::tan(light.outer_cone_angle)));
    return ret;
}

//! relative selection-weight for importance-sampled light-picking. luminance times a geometric
//! measure: emitter flux for lights with extent, unit solid angle for delta lights.
//! never used for shading, only to build a selection-distribution - the absolute scale is arbitrary.
static inline float light_power(const light_t &light)
{
    float measure = 1.f;

    switch(static_cast<LightType>(light.type))
    {
        // sun-disc: cap solid angle. angular_size is the cap half-angle, as sample_light() uses it
        case LightType::Directional:
            if(light.angular_size > 0.f) { measure = glm::two_pi<float>() * (1.f - std::cos(light.angular_size)); }
            break;

        // delta position, no extent: unit solid angle
        case LightType::Omni:
        case LightType::Spot: break;

        // area emitters: pi * area. extents match the area-pdfs in sample_light()
        case LightType::Rect: measure = glm::pi<float>() * 4.f * light.size_x * light.size_y; break;
        case LightType::Disk: measure = glm::pi<float>() * glm::pi<float>() * light.size_x * light.size_x; break;
        case LightType::Sphere:
            measure = glm::pi<float>() * 4.f * glm::pi<float>() * light.size_x * light.size_x;
            break;
        case LightType::Tube: measure = glm::pi<float>() * 4.f * glm::pi<float>() * light.size_x * light.size_y; break;

        // reserved for extracted emissive triangles
        case LightType::Area: break;
    }

    // NTSC luma, matching utils::LuminanceNTSC in the shaders
    const glm::vec3 luma = {0.299f, 0.587f, 0.114f};
    return glm::dot(light.color, luma) * light.intensity * measure;
}

//! one bucket of a Vose alias-table, picking a light in O(1) from two uniform draws
struct light_alias_bin_t
{
    //! index picked when the threshold-draw fails
    uint32_t alias = 0;

    //! probability of keeping this bin's own index instead of 'alias'
    float threshold = 1.f;

    //! selection-probability of this bin's own index. needed for the MIS pdf-lookup
    float prob = 0.f;
};

static_assert(sizeof(light_alias_bin_t) == 12, "light_alias_bin_t layout must match shader-side (ray_common.slang)");

/**
 * @brief   create_light_alias_table builds a Vose alias-table over the lights' selection-weights.
 *
 * zero-weight lights get probability 0 and are never picked, which is also what the hit-side MIS
 * needs to give bsdf-sampling full weight for them. if no light carries weight the table
 * degenerates to a uniform distribution, so it stays samplable.
 *
 * @param   lights  a provided array of lights
 * @return  one bin per light
 */
static inline std::vector<light_alias_bin_t> create_light_alias_table(const std::vector<light_t> &lights)
{
    const size_t num_lights = lights.size();
    std::vector<light_alias_bin_t> bins(num_lights);
    if(!num_lights) { return bins; }

    std::vector<float> weights(num_lights);
    double total = 0.0;

    for(size_t i = 0; i < num_lights; ++i)
    {
        weights[i] = std::max(0.f, light_power(lights[i]));
        total += weights[i];
    }
    const bool uniform = total <= 0.0;

    // scaled probabilities with mean 1, partitioned into under- and overfull bins
    std::vector<double> scaled(num_lights);
    std::vector<uint32_t> small, large;

    for(size_t i = 0; i < num_lights; ++i)
    {
        bins[i].alias = static_cast<uint32_t>(i);
        bins[i].prob = uniform ? 1.f / num_lights : static_cast<float>(weights[i] / total);
        scaled[i] = uniform ? 1.0 : num_lights * weights[i] / total;
        if(scaled[i] < 1.0) { small.push_back(static_cast<uint32_t>(i)); }
        else { large.push_back(static_cast<uint32_t>(i)); }
    }

    while(!small.empty() && !large.empty())
    {
        uint32_t s = small.back(), l = large.back();
        small.pop_back();
        large.pop_back();

        bins[s].threshold = static_cast<float>(scaled[s]);
        bins[s].alias = l;

        scaled[l] -= 1.0 - scaled[s];
        if(scaled[l] < 1.0) { small.push_back(l); }
        else { large.push_back(l); }
    }

    // whatever is left is 1 up to rounding, and keeps its self-alias
    return bins;
}

//! draw an index from a Vose alias-table. u1/u2 are independent uniforms in [0, 1).
//! the shader-side pick must match this exactly
static inline uint32_t sample_light_alias_table(const std::vector<light_alias_bin_t> &bins, float u1, float u2)
{
    if(bins.empty()) { return 0; }
    auto index = std::min(static_cast<uint32_t>(u1 * bins.size()), static_cast<uint32_t>(bins.size() - 1));
    return u2 < bins[index].threshold ? index : bins[index].alias;
}

}// namespace vierkant
