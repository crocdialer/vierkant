//
// Created by crocdialer on 2/21/20.
//

#include <deque>
#include "vierkant/nodes.hpp"

namespace vierkant::nodes
{

static inline void bfs(const NodeConstPtr &root, const std::function<void(const NodeConstPtr&)> &fn)
{
    std::deque<vierkant::nodes::NodeConstPtr> node_queue;
    node_queue.emplace_back(root);

    while(!node_queue.empty())
    {
        auto node = std::move(node_queue.front());
        node_queue.pop_front();

        if(fn){ fn(node); }

        // queue all children
        for(auto &child_node: node->children){ node_queue.emplace_back(child_node); }
    }
}

static inline void dfs(const NodeConstPtr &root, const std::function<void(const NodeConstPtr&)> &fn)
{

}

//! recursion helper-routine
void build_node_matrices_helper(const NodeConstPtr &node,
                                const node_animation_t &animation,
                                std::vector<glm::mat4> &matrices,
                                glm::mat4 global_joint_transform);

uint32_t num_nodes_in_hierarchy(const NodeConstPtr &root)
{
    if(!root){ return 0; }
    uint32_t ret = 1;
    for(const auto &b: root->children){ ret += num_nodes_in_hierarchy(b); }
    return ret;
}

NodeConstPtr node_by_name(NodeConstPtr root, const std::string &name)
{
    if(!root){ return nullptr; }
    if(root->name == name){ return root; }
    for(const auto &c: root->children)
    {
        auto b = node_by_name(c, name);
        if(b){ return b; }
    }
    return nullptr;
}

void build_morph_weights_bfs(const NodeConstPtr &root,
                             const node_animation_t &animation,
                             float animation_time,
                             std::vector<std::vector<float>> &morph_weights)
{
    if(!root){ return; }
    morph_weights.resize(num_nodes_in_hierarchy(root));

    bfs(root, [&morph_weights, &animation, animation_time](auto &node)
    {
        auto it = animation.keys.find(node);

        if(it != animation.keys.end())
        {
            const auto &animation_keys = it->second;

            if(!animation_keys.morph_weights.empty())
            {
                morph_weights[node->index] = create_morph_weights(animation_keys, animation_time,
                                                                  animation.interpolation_mode);
            }
        }
    });
}

void build_node_matrices_bfs(const NodeConstPtr &root,
                             const node_animation_t &animation,
                             float animation_time,
                             std::vector<glm::mat4> &matrices)
{
    if(!root){ return; }
    matrices.resize(num_nodes_in_hierarchy(root));

    std::deque<std::pair<vierkant::nodes::NodeConstPtr, glm::mat4>> node_queue;
    node_queue.emplace_back(root, glm::mat4(1));

    while(!node_queue.empty())
    {
        auto[node, global_joint_transform] = node_queue.front();
        node_queue.pop_front();

        glm::mat4 node_transform = node->transform;
        auto it = animation.keys.find(node);

        if(it != animation.keys.end())
        {
            const auto &animation_keys = it->second;
            create_animation_transform(animation_keys, animation_time, animation.interpolation_mode,
                                       node_transform);
        }
        global_joint_transform = global_joint_transform * node_transform;

        // add final transform
        matrices[node->index] = global_joint_transform * node->offset;

        // queue all children
        for(auto &child_node: node->children){ node_queue.emplace_back(child_node, global_joint_transform); }
    }
}

void build_node_matrices(const NodeConstPtr &root, const node_animation_t &animation, std::vector<glm::mat4> &matrices)
{
    if(!root){ return; }
    matrices.resize(num_nodes_in_hierarchy(root));
    build_node_matrices_helper(root, animation, matrices, glm::mat4(1));
}

void build_node_matrices_helper(const NodeConstPtr &node,
                                const node_animation_t &animation,
                                float animation_time,
                                std::vector<glm::mat4> &matrices,
                                glm::mat4 global_joint_transform)
{
    glm::mat4 node_transform = node->transform;

    auto it = animation.keys.find(node);

    if(it != animation.keys.end())
    {
        const auto &animation_keys = it->second;
        create_animation_transform(animation_keys, animation_time, animation.interpolation_mode, node_transform);
    }
    global_joint_transform = global_joint_transform * node_transform;

    // add final transform
    matrices[node->index] = global_joint_transform * node->offset;

    // recursion through all children
    for(auto &b: node->children){ build_node_matrices_helper(b, animation, animation_time, matrices, global_joint_transform); }
}

}