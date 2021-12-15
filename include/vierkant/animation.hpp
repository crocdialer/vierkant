//
// Created by crocdialer on 3/13/20.
//

#pragma once

#include <vector>
#include <map>
#include <vierkant/math.hpp>

namespace vierkant
{

enum class InterpolationMode
{
    Linear,
    Step,
    CubicSpline
};

template<typename T>
struct animation_value_t
{
    T in_tangent = T(0);
    T value = T(0);
    T out_tangent = T(0);
};

/**
 *  @brief  animation_keys_t groups all existing keys for an entity.
 */
struct animation_keys_t
{
    std::map<float, glm::vec3> positions;
    std::map<float, glm::quat> rotations;
    std::map<float, glm::vec3> scales;
};

/**
 * @brief   animation_t groups all information for a keyframe animation.
 *
 * @tparam  T   value type
 */
template<typename T>
struct animation_t
{
    std::string name;
    bool playing = true;
    float current_time = 0.f;
    float duration = 0.f;
    float ticks_per_sec = 1.f;
    std::map<T, animation_keys_t> keys;
    InterpolationMode interpolation_mode = InterpolationMode::Linear;
};

template<typename T>
void update_animation(animation_t<T> &animation, float time_delta, float animation_speed)
{
    if(animation.playing)
    {
        animation.current_time = animation.current_time + time_delta * animation.ticks_per_sec * animation_speed;
        if(animation.current_time > animation.duration){ animation.current_time -= animation.duration; }
        animation.current_time += animation.current_time < 0.f ? animation.duration : 0.f;
    }
}

/**
 * @brief   Evaluate provided animation-keys for a given time. If successful, write out transformation.
 *
 * @param   keys            the animation-keys to evaluate.
 *
 * @param   time            provided time for interpolation.
 *
 * @param   out_transform   ref to a mat4, used to write out an interpolated transformation.
 */
void create_animation_transform(const animation_keys_t &keys,
                                float time,
                                InterpolationMode interpolation_mode,
                                glm::mat4 &out_transform);

}
