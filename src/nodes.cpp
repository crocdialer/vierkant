//
// Created by crocdialer on 2/21/20.
//

#include "vierkant/nodes.hpp"

namespace vierkant::nodes
{

//! recursion helper-routine
void build_bone_matrices_helper(const NodeConstPtr &bone,
                                const node_animation_t &animation,
                                std::vector<glm::mat4> &matrices,
                                glm::mat4 transform);

uint32_t num_nodes_in_hierarchy(const NodeConstPtr &root)
{
    if(!root){ return 0; }
    uint32_t ret = 1;
    for(const auto &b : root->children){ ret += num_nodes_in_hierarchy(b); }
    return ret;
}

NodeConstPtr node_by_name(NodeConstPtr root, const std::string &name)
{
    if(!root){ return nullptr; }
    if(root->name == name){ return root; }
    for(const auto &c : root->children)
    {
        auto b = node_by_name(c, name);
        if(b){ return b; }
    }
    return nullptr;
}

void build_node_matrices(const NodeConstPtr &root, const node_animation_t &animation, std::vector<glm::mat4> &matrices)
{
    if(!root){ return; }
    matrices.resize(num_nodes_in_hierarchy(root));
    return build_bone_matrices_helper(root, animation, matrices, glm::mat4(1));
}

void build_bone_matrices_helper(const NodeConstPtr &bone,
                                const node_animation_t &animation,
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