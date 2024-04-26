//
// Created by crocdialer on 3/13/20.
//

#pragma once

#include <map>
#include <string>
#include <vector>
#include <vierkant/math.hpp>
#include <vierkant/object_component.hpp>
#include <vierkant/transform.hpp>

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
template<typename T = float>
    requires std::floating_point<T>
struct animation_keys_t_
{
    std::map<T, animation_value_t<glm::vec3>> positions;
    std::map<T, animation_value_t<glm::quat>> rotations;
    std::map<T, animation_value_t<glm::vec3>> scales;
    std::map<T, animation_value_t<std::vector<double>>> morph_weights;
};
using animation_keys_t = animation_keys_t_<float>;

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

/**
 * @brief   animation_component_t_ is a struct-template to store an entity's animation-state.
 *
 * support for comparing and hashing
 * @tparam  T   a floating-point template param
 */
template<std::floating_point T = float>
struct animation_component_t_
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! index into an array of animations
    uint32_t index = 0;

    //! true if animation is playing
    bool playing = true;

    //! scaling factor for animation-speed
    T animation_speed = 1.0;

    //! current time
    T current_time = 0.0;

    bool operator==(const animation_component_t_ &other) const = default;
};
using animation_component_t = animation_component_t_<float>;

template<typename T>
void update_animation(const animation_t<T> &animation, double time_delta,
                      vierkant::animation_component_t &animation_state)
{
    if(animation_state.playing)
    {
        animation_state.current_time += time_delta * animation.ticks_per_sec * animation_state.animation_speed;
        if(animation_state.current_time > animation.duration) { animation_state.current_time -= animation.duration; }
        animation_state.current_time += animation_state.current_time < 0.f ? animation.duration : 0.f;
    }
}

/**
 * @brief   Evaluate provided animation-keys for a given time. If successful, write out transformation.
 *
 * @param   keys            the animation-keys to evaluate.
 * @param   time            provided time for interpolation.
 * @param   out_transform   ref to a mat4, used to write out an interpolated transformation.
 * @return  true if a transformation was successfully written.
 */
bool create_animation_transform(const animation_keys_t &keys, float time, InterpolationMode interpolation_mode,
                                vierkant::transform_t &out_transform);

/**
 * @brief   Evaluate provided animation-keys for a given time. If successful, write out transformation.
 *
 * @param   keys            the animation-keys to evaluate.
 * @param   time            provided time for interpolation.
 * @param   out_transform   ref to an vector to store calculated weights.
 * @return  true if any morph-weights were written.
 */
bool create_morph_weights(const animation_keys_t &keys, float time, InterpolationMode interpolation_mode,
                          std::vector<double> &out_weights);
}// namespace vierkant

namespace std
{

template<typename T>
struct hash<vierkant::animation_component_t_<T>>
{
    size_t operator()(vierkant::animation_component_t_<T> const &animation_state) const;
};

}// namespace std
