//
// Created by crocdialer on 2/21/20.
//

#include "vierkant/bones.hpp"

namespace vierkant::bones
{

//! recursion helper-routine
void build_bone_matrices_helper(const BoneConstPtr &bone,
                                const animation_t &animation,
                                std::vector<glm::mat4> &matrices,
                                glm::mat4 transform);

uint32_t num_bones_in_hierarchy(const BoneConstPtr &root)
{
    if(!root){ return 0; }
    uint32_t ret = 1;
    for(const auto &b : root->children){ ret += num_bones_in_hierarchy(b); }
    return ret;
}

BoneConstPtr bone_by_name(BoneConstPtr root, const std::string &name)
{
    if(!root){ return nullptr; }
    if(root->name == name){ return root; }
    for(const auto &c : root->children)
    {
        auto b = bone_by_name(c, name);
        if(b){ return b; }
    }
    return nullptr;
}

void build_bone_matrices(const BoneConstPtr &root, const animation_t &animation, std::vector<glm::mat4> &matrices)
{
    if(!root){ return; }
    matrices.resize(num_bones_in_hierarchy(root));
    return build_bone_matrices_helper(root, animation, matrices, glm::mat4(1));
}

void build_bone_matrices_helper(const BoneConstPtr &bone,
                                const animation_t &animation,
                                std::vector<glm::mat4> &matrices,
                                glm::mat4 world_transform)
{
    float time = animation.current_time;
    glm::mat4 boneTransform = bone->transform;

    auto it = animation.bone_keys.find(bone);

    if(it == animation.bone_keys.end()){ return; }

    const auto &bonekeys = it->second;

    bool boneHasKeys = false;

    // translation
    glm::mat4 translation(1);

    if(!bonekeys.position_keys.empty())
    {
        boneHasKeys = true;
        uint32_t i = 0;

        for(; i < bonekeys.position_keys.size() - 1; i++)
        {
            const auto &key = bonekeys.position_keys[i + 1];
            if(key.time >= time){ break; }
        }
        // i now holds the correct time index
        const key_t<glm::vec3> &key1 = bonekeys.position_keys[i];
        const key_t<glm::vec3> &key2 = bonekeys.position_keys[(i + 1) % bonekeys.position_keys.size()];

        float startTime = key1.time;
        float endTime = key2.time < key1.time ? key2.time + animation.duration : key2.time;
        float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);
        glm::vec3 pos = glm::mix(key1.value, key2.value, frac);
        translation = glm::translate(translation, pos);
    }

    // rotation
    glm::mat4 rotation(1);
    if(!bonekeys.rotation_keys.empty())
    {
        boneHasKeys = true;
        uint32_t i = 0;
        for(; i < bonekeys.rotation_keys.size() - 1; i++)
        {
            const key_t<glm::quat> &key = bonekeys.rotation_keys[i + 1];
            if(key.time >= time){ break; }
        }
        // i now holds the correct time index
        const key_t<glm::quat> &key1 = bonekeys.rotation_keys[i];
        const key_t<glm::quat> &key2 = bonekeys.rotation_keys[(i + 1) % bonekeys.rotation_keys.size()];

        float startTime = key1.time;
        float endTime = key2.time < key1.time ? key2.time + animation.duration : key2.time;
        float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);

        // quaternion spherical linear interpolation
        glm::quat interpolRot = glm::slerp(key1.value, key2.value, frac);
        rotation = glm::mat4_cast(interpolRot);
    }

    // scale
    glm::mat4 scaleMatrix(1);

    if(!bonekeys.scale_keys.empty())
    {
        if(bonekeys.scale_keys.size() == 1)
        {
            scaleMatrix = glm::scale(scaleMatrix, bonekeys.scale_keys.front().value);
        }
        else
        {
            boneHasKeys = true;
            uint32_t i = 0;

            for(; i < bonekeys.scale_keys.size() - 1; i++)
            {
                const key_t<glm::vec3> &key = bonekeys.scale_keys[i + 1];
                if(key.time >= time){ break; }
            }
            // i now holds the correct time index
            const key_t<glm::vec3> &key1 = bonekeys.scale_keys[i];
            const key_t<glm::vec3> &key2 = bonekeys.scale_keys[(i + 1) % bonekeys.scale_keys.size()];

            float startTime = key1.time;
            float endTime = key2.time < key1.time ? key2.time + animation.duration : key2.time;
            float frac = std::max((time - startTime) / (endTime - startTime), 0.0f);
            glm::vec3 scale = glm::mix(key1.value, key2.value, frac);
            scaleMatrix = glm::scale(scaleMatrix, scale);
        }
    }
    if(boneHasKeys){ boneTransform = translation * rotation * scaleMatrix; }
    world_transform = world_transform * boneTransform;

    // add final transform
    matrices[bone->index] = world_transform * bone->offset;

    // recursion through all children
    for(auto &b : bone->children){ build_bone_matrices_helper(b, animation, matrices, world_transform); }
}

}