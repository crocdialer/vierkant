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

        if(keys.positions.size() == 1)
        {
            translation = glm::translate(translation, keys.positions.front().value);
        }
        else
        {
            uint32_t i = 0;

            for(; i < keys.positions.size() - 1; i++)
            {
                const auto &key = keys.positions[(i + 1) % keys.positions.size()];
                if(key.time >= time){ break; }
            }
            // i now holds the correct time index
            const key_t<glm::vec3> &key1 = keys.positions[i];
            const key_t<glm::vec3> &key2 = keys.positions[(i + 1) % keys.positions.size()];

            float startTime = key1.time;
            float endTime = key2.time < key1.time ? key2.time + duration : key2.time;
            float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);
            glm::vec3 pos = glm::mix(key1.value, key2.value, frac);
            translation = glm::translate(translation, pos);
        }
    }

    // rotation
    glm::mat4 rotation(1);
    if(!keys.rotations.empty())
    {
        has_keys = true;

        if(keys.rotations.size() == 1)
        {
            rotation = glm::mat4_cast(keys.rotations.front().value);
        }
        else
        {
            uint32_t i = 0;
            for(; i < keys.rotations.size() - 1; i++)
            {
                const key_t<glm::quat> &key = keys.rotations[i + 1];
                if(key.time >= time){ break; }
            }
            // i now holds the correct time index
            const key_t<glm::quat> &key1 = keys.rotations[i];
            const key_t<glm::quat> &key2 = keys.rotations[(i + 1) % keys.rotations.size()];

            float startTime = key1.time;
            float endTime = key2.time < key1.time ? key2.time + duration : key2.time;
            float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);

            // quaternion spherical linear interpolation
            glm::quat interpolRot = glm::slerp(key1.value, key2.value, frac);
            rotation = glm::mat4_cast(interpolRot);
        }
    }

    // scale
    glm::mat4 scaleMatrix(1);

    if(!keys.scales.empty())
    {
        has_keys = true;

        if(keys.scales.size() == 1)
        {
            scaleMatrix = glm::scale(scaleMatrix, keys.scales.front().value);
        }
        else
        {
            uint32_t i = 0;

            for(; i < keys.scales.size() - 1; i++)
            {
                const key_t<glm::vec3> &key = keys.scales[(i + 1) % keys.scales.size()];
                if(key.time >= time){ break; }
            }
            // i now holds the correct time index
            const key_t<glm::vec3> &key1 = keys.scales[i];
            const key_t<glm::vec3> &key2 = keys.scales[(i + 1) % keys.scales.size()];

            float startTime = key1.time;
            float endTime = key2.time < key1.time ? key2.time + duration : key2.time;
            float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);
            glm::vec3 scale = glm::mix(key1.value, key2.value, frac);
            scaleMatrix = glm::scale(scaleMatrix, scale);
        }
    }
    if(has_keys){ out_transform = translation * rotation * scaleMatrix; }
}

}