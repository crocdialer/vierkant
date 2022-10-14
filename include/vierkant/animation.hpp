//
// Created by crocdialer on 3/13/20.
//

#pragma once

#include <vector>
#include <map>
#include <vierkant/math.hpp>

namespace vierkant
{

//! InterpolationMode defines available interpolation-modes for animations.
enum class InterpolationMode
{
    Linear,
    Step,
    CubicSpline
};

/**
 *  @brief  animation_value_t can be used to store animation data-points.
 *          optionally stores in- and out-tangents that can be used for cubic hermite-interpolation.
 */
template<typename T>
struct animation_value_t
{
    T value;
    T in_tangent;
    T out_tangent;
};

/**
 *  @brief  animation_keys_t groups all existing keys for an entity.
 */
struct animation_keys_t
{
    std::map<float, animation_value_t<glm::vec3>> positions;
    std::map<float, animation_value_t<glm::quat>> rotations;
    std::map<float, animation_value_t<glm::vec3>> scales;
    std::map<float, animation_value_t<std::vector<float>>> morph_weights;
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
    float duration = 0.f;
    float ticks_per_sec = 1.f;
    std::map<T, animation_keys_t> keys;
    InterpolationMode interpolation_mode = InterpolationMode::Linear;
};

struct animation_state_t
{
    uint32_t index = 0;
    bool playing = true;
    double animation_speed = 1.0;
    double current_time = 0.0;
};

template<typename T>
void update_animation(const animation_t<T> &animation,
                      float time_delta,
                      vierkant::animation_state_t &animation_state)
{
    if(animation_state.playing)
    {
        animation_state.current_time =
                animation_state.current_time + time_delta * animation.ticks_per_sec * animation_state.animation_speed;
        if(animation_state.current_time > animation.duration){ animation_state.current_time -= animation.duration; }
        animation_state.current_time += animation_state.current_time < 0.f ? animation.duration : 0.f;
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

std::vector<float> create_morph_weights(const animation_keys_t &keys,
                                        float time,
                                        InterpolationMode interpolation_mode);
}
