//
// Created by crocdialer on 3/13/20.
//

#pragma once

#include <map>
#include <string>
#include <vector>
#include <vierkant/math.hpp>
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

template<typename T = float>
    requires std::floating_point<T>
struct animation_state_t_
{
    // object_component concept
    static constexpr char component_description[] = "animation state";

    uint32_t index = 0;
    bool playing = true;
    T animation_speed = 1.0;
    T current_time = 0.0;
};
using animation_state_t = animation_state_t_<float>;

template<typename T>
bool operator==(const animation_state_t_<T> &lhs, const animation_state_t_<T> &rhs);

template<typename T>
inline bool operator!=(const animation_state_t_<T> &lhs, const animation_state_t_<T> &rhs)
{
    return !(lhs == rhs);
}

template<typename T>
void update_animation(const animation_t<T> &animation, double time_delta, vierkant::animation_state_t &animation_state)
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
struct hash<vierkant::animation_state_t_<T>>
{
    size_t operator()(vierkant::animation_state_t_<T> const &animation_state) const;
};

}// namespace std
