//
// Created by crocdialer on 3/13/20.
//

#pragma once

#include <vector>
#include <map>
#include <vierkant/math.hpp>

namespace vierkant
{

/**
 * @brief   key_t groups an arbitrary value with a specific point in time.
 *
 * @tparam  T   value type
 */
template<typename T>
struct key_t
{
    float time = 0.f;
    T value = T(0);
};

/**
 *  @brief  animation_keys_t groups all existing keys for an entity.
 */
struct animation_keys_t
{
    std::vector<key_t<glm::vec3>> positions;
    std::vector<key_t<glm::quat>> rotations;
    std::vector<key_t<glm::vec3>> scales;
};

/**
 * @brief   animation_t groups all information for a keyframe animation.
 *
 * @tparam  T   value type
 */
template<typename T>
struct animation_t
{
    float current_time = 0.f;
    float duration = 0.f;
    float ticks_per_sec = 0.f;
    std::map<T, animation_keys_t> keys;
};

/**
 * @brief   Evaluate provided animation-keys for a given time and duration. If successful, write out transformation.
 *
 * @param   keys            the animation-keys to evaluate.
 *
 * @param   time            provided time for interpolation.
 *
 * @param   duration        the animation duration.
 *
 * @param   out_transform   ref to a mat4, used to write out an interpolated transformation.
 */
void create_animation_transform(const animation_keys_t &keys, float time, float duration, glm::mat4 &out_transform);

}