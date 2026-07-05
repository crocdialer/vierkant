//
// Created by crocdialer on 4/13/22.
//

#pragma once

#include <limits>
#include <string>

#include <crocore/NamedUUID.hpp>
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
};
static_assert(sizeof(light_t) == 80, "light_t layout must match shader-side (ray_common.slang)");

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
    return ret;
}

}// namespace vierkant
