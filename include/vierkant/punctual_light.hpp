//
// Created by crocdialer on 4/13/22.
//

#pragma once

#include "vierkant/model/model_loading.hpp"
#include <limits>
#include <vierkant/math.hpp>

namespace vierkant
{

//! lightsource object component
struct lightsource_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    vierkant::model::LightType type = vierkant::model::LightType::Omni;
    glm::vec3 color = glm::vec3(1);
    float intensity = 1.f;
    float range = std::numeric_limits<float>::infinity();
    float inner_cone_angle = 0.f;
    float outer_cone_angle = glm::quarter_pi<float>();
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
    float pad;
};

static inline light_t convert_light(const vierkant::lightsource_component_t &light_cmp,
                                    const vierkant::transform_t &t)
{
    light_t ret = {};
    ret.position = t.translation;
    ret.type = static_cast<uint32_t>(light_cmp.type);
    ret.color = light_cmp.color;
    ret.intensity = light_cmp.intensity;
    ret.direction = t.rotation * glm::vec3(0.f, 0.f, -1.f);
    ret.range = light_cmp.range > 0.f ? light_cmp.range : std::numeric_limits<float>::infinity();

    ret.spot_angle_scale =
            1.f / std::max(0.001f, std::cos(light_cmp.inner_cone_angle) - std::cos(light_cmp.outer_cone_angle));
    ret.spot_angle_offset = -std::cos(light_cmp.outer_cone_angle) * ret.spot_angle_scale;
    return ret;
}

}// namespace vierkant
