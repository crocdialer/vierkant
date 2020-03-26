//
// Created by crocdialer on 3/13/20.
//

#include <vierkant/animation.hpp>

namespace vierkant
{

void create_animation_transform(const animation_keys_t &keys, float time, float duration, glm::mat4 &out_transform)
{
    bool has_keys = false;

    // translation
    glm::mat4 translation(1);

    if(!keys.positions.empty())
    {
        has_keys = true;

        // find a key with equal or greater time
        auto it_rhs = keys.positions.lower_bound(time);

        // lhs iterator (might be invalid)
        auto it_lhs = it_rhs; it_lhs--;

        if(it_rhs == keys.positions.begin())
        {
            // time is before first key
            translation = glm::translate(translation, it_rhs->second);
        }
        else if(it_rhs == keys.positions.end())
        {
            // time is past last key
            translation = glm::translate(translation, it_lhs->second);
        }
        else
        {
            // interpolate two surrounding keys
            float startTime = it_lhs->first;
            float endTime = it_rhs->first;
            float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);
            glm::vec3 pos = glm::mix(it_lhs->second, it_rhs->second, frac);
            translation = glm::translate(translation, pos);
        }
    }

    // rotation
    glm::mat4 rotation(1);
    if(!keys.rotations.empty())
    {
        has_keys = true;

        // find a key with equal or greater time
        auto it_rhs = keys.rotations.lower_bound(time);

        // lhs iterator (might be invalid)
        auto it_lhs = it_rhs; it_lhs--;

        if(it_rhs == keys.rotations.begin())
        {
            // time is before first key
            rotation = glm::mat4_cast(it_rhs->second);
        }
        else if(it_rhs == keys.rotations.end())
        {
            // time is past last key
            rotation = glm::mat4_cast(it_lhs->second);
        }
        else
        {
            // interpolate two surrounding keys
            float startTime = it_lhs->first;
            float endTime = it_rhs->first;
            float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);

            // quaternion spherical linear interpolation
            glm::quat interpolRot = glm::slerp(it_lhs->second, it_rhs->second, frac);
            rotation = glm::mat4_cast(interpolRot);
        }
    }

    // scale
    glm::mat4 scale_matrix(1);

    if(!keys.scales.empty())
    {
        has_keys = true;

        // find a key with equal or greater time
        auto it_rhs = keys.scales.lower_bound(time);

        // lhs iterator (might be invalid)
        auto it_lhs = it_rhs; it_lhs--;

        if(it_rhs == keys.scales.begin())
        {
            // time is before first key
            scale_matrix = glm::scale(scale_matrix, it_rhs->second);
        }
        else if(it_rhs == keys.scales.end())
        {
            // time is past last key
            scale_matrix = glm::scale(scale_matrix, it_lhs->second);
        }
        else
        {
            // interpolate two surrounding keys
            float startTime = it_lhs->first;
            float endTime = it_rhs->first;
            float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);
            glm::vec3 scale = glm::mix(it_lhs->second, it_rhs->second, frac);
            scale_matrix = glm::scale(scale_matrix, scale);
        }
    }
    if(has_keys){ out_transform = translation * rotation * scale_matrix; }
}

}