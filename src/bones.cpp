//
// Created by crocdialer on 2/21/20.
//

#include "vierkant/bones.hpp"

namespace vierkant::bones
{

//! recursion helper-routine
void build_bone_matrices_helper(const BoneConstPtr &bone,
                                const bone_animation_t &animation,
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

void build_bone_matrices(const BoneConstPtr &root, const bone_animation_t &animation, std::vector<glm::mat4> &matrices)
{
    if(!root){ return; }
    matrices.resize(num_bones_in_hierarchy(root));
    return build_bone_matrices_helper(root, animation, matrices, glm::mat4(1));
}

void build_bone_matrices_helper(const BoneConstPtr &bone,
                                const bone_animation_t &animation,
                                std::vector<glm::mat4> &matrices,
                                glm::mat4 world_transform)
{
    float time = animation.current_time;
    glm::mat4 boneTransform = bone->transform;

    auto it = animation.keys.find(bone);

    if(it != animation.keys.end())
    {
        const auto &animation_keys = it->second;
        create_animation_transform(animation_keys, time, animation.duration, boneTransform);
    }
    world_transform = world_transform * boneTransform;

    auto bone_matrix = world_transform * bone->offset;

    // add final transform
    matrices[bone->index] = bone_matrix;

    // recursion through all children
    for(auto &b : bone->children){ build_bone_matrices_helper(b, animation, matrices, world_transform); }
}

}