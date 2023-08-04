//
// Created by crocdialer on 2/21/20.
//

#include "vierkant/nodes.hpp"
#include <algorithm>
#include <deque>

namespace vierkant::nodes
{

using traversal_fn = std::function<bool(const NodeConstPtr &)>;

static inline void bfs(const NodeConstPtr &root, const traversal_fn &fn)
{
    if(!root) { return; }

    std::deque<vierkant::nodes::NodeConstPtr> node_queue;
    node_queue.emplace_back(root);

    while(!node_queue.empty())
    {
        auto node = std::move(node_queue.front());
        node_queue.pop_front();

        // evaluate traversal-function, optionally abort traversal
        if(fn && !fn(node)) { return; }

        // queue all children
        for(auto &child_node: node->children) { node_queue.emplace_back(child_node); }
    }
}

uint32_t num_nodes_in_hierarchy(const NodeConstPtr &root)
{
    uint32_t ret = 0;
    bfs(root, [&ret](const NodeConstPtr &) {
        ret++;
        return true;
    });
    return ret;
}

NodeConstPtr node_by_name(const NodeConstPtr &root, const std::string &name)
{
    NodeConstPtr ret;
    bfs(root, [&name, &ret](const NodeConstPtr &node) {
        if(node->name == name)
        {
            ret = node;
            return false;
        }
        return true;
    });
    return ret;
}

template<typename T, typename>
void build_morph_weights_bfs(const NodeConstPtr &root, const node_animation_t &animation, float time,
                             std::vector<std::vector<T>> &morph_weights)
{
    if(!root) { return; }
    morph_weights.resize(num_nodes_in_hierarchy(root));

    bfs(root, [&morph_weights, &animation, time](auto &node) {
        auto it = animation.keys.find(node);

        if(it != animation.keys.end())
        {
            const auto &animation_keys = it->second;

            if(!animation_keys.morph_weights.empty())
            {
                std::vector<double> tmp_weights;
                create_morph_weights(animation_keys, time, animation.interpolation_mode, tmp_weights);
                morph_weights[node->index].resize(tmp_weights.size());
                std::transform(tmp_weights.begin(), tmp_weights.end(), morph_weights[node->index].begin(),
                               [](double w) -> T { return static_cast<T>(w); });
            }
        }
        return true;
    });
}

// explicit template-specializations
template void build_morph_weights_bfs(const NodeConstPtr &root, const node_animation_t &animation, float time,
                                      std::vector<std::vector<float>> &morph_weights);

template void build_morph_weights_bfs(const NodeConstPtr &root, const node_animation_t &animation, float time,
                                      std::vector<std::vector<double>> &morph_weights);

void build_node_matrices_bfs(const NodeConstPtr &root, const node_animation_t &animation, float time,
                             std::vector<vierkant::transform_t> &transforms)
{
    if(!root) { return; }
    transforms.resize(num_nodes_in_hierarchy(root));

    std::deque<std::pair<vierkant::nodes::NodeConstPtr, vierkant::transform_t>> node_queue;
    node_queue.emplace_back(root, vierkant::transform_t{});

    while(!node_queue.empty())
    {
        auto [node, global_joint_transform] = node_queue.front();
        node_queue.pop_front();

        auto node_transform = node->transform;
        auto it = animation.keys.find(node);

        if(it != animation.keys.end())
        {
            const auto &animation_keys = it->second;
            create_animation_transform(animation_keys, time, animation.interpolation_mode, node_transform);
        }
        global_joint_transform = global_joint_transform * node_transform;

        // add final transform
        transforms[node->index] = global_joint_transform * node->offset;

        // queue all children
        for(auto &child_node: node->children) { node_queue.emplace_back(child_node, global_joint_transform); }
    }
}

}// namespace vierkant::nodes