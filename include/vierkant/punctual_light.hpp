//
// Created by crocdialer on 4/13/22.
//

#pragma once

#include <vierkant/math.hpp>
#include <limits>

namespace vierkant
{

enum LightType : uint32_t
{
    Point = 0,
    Spot,
    Directional
};

struct lightsource_t
{
    glm::vec3 position;
    LightType type = LightType::Point;
    glm::vec3 color = glm::vec3(1);
    float intensity = 1.f;
    glm::vec3 direction = glm::vec3(0.f, 0.f, -1.f);
    float range = std::numeric_limits<float>::infinity();
    float inner_cone_angle = 0.f;
    float outer_cone_angle = glm::quarter_pi<float>();
};

struct alignas(16) lightsource_ubo_t
{
    glm::vec3 position;
    uint32_t type;
    glm::vec3 color;
    float intensity;
    glm::vec3 direction;
    float range;
    float spot_angle_scale;
    float spot_angle_offset;
};

static inline lightsource_ubo_t convert_light(const lightsource_t &light_in)
{
    lightsource_ubo_t ret = {};
    ret.position = light_in.position;
    ret.type = light_in.type;
    ret.color = light_in.color;
    ret.intensity = light_in.intensity;
    ret.direction = light_in.direction;
    ret.range = light_in.range > 0.f ? light_in.range : std::numeric_limits<float>::infinity();

    ret.spot_angle_scale =
            1.f / std::max(0.001f, std::cos(light_in.inner_cone_angle) - std::cos(light_in.outer_cone_angle));
    ret.spot_angle_offset = -std::cos(light_in.outer_cone_angle) * ret.spot_angle_scale;
    return ret;
}

}// namespace vierkant
