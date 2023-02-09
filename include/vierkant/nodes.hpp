//
// Created by crocdialer on 2/21/20.
//

#pragma once

#include <list>
#include <map>
#include <memory>
#include <vierkant/animation.hpp>
#include <vierkant/math.hpp>
#include <vierkant/transform.hpp>

namespace vierkant::nodes
{

using NodePtr = std::shared_ptr<struct node_t>;
using NodeConstPtr = std::shared_ptr<const struct node_t>;

struct node_t
{
    std::string name;
    vierkant::transform_t transform = {};
    vierkant::transform_t offset = {};
    uint32_t index = 0;
    NodePtr parent = nullptr;
    std::list<NodePtr> children;
};

//! define a bone_animation type
using node_animation_t = vierkant::animation_t<NodeConstPtr>;

/**
 * @brief   Return the total number of nodes.
 *
 * @param   root    the root-node of a hierarchy.
 * @return  the total number of nodes.
 */
uint32_t num_nodes_in_hierarchy(const NodeConstPtr &root);

/**
 * @brief   Attempt to find a node by name.
 *
 * @param   root    a root bone of a node-hierarchy.
 * @param   name    the name to search for.
 * @return  the found NodePtr or nullptr, if the name could not be found in the hierarchy.
 */
NodeConstPtr node_by_name(const NodeConstPtr& root, const std::string &name);

/**
 * @brief   Create transformation matrices, matching the provided node-hierarchy and animation.
 *
 * @param   root        a root node of a node-hierarchy.
 * @param   animation   a const-ref for an animation_t object.
 * @param   time        current time.
 * @param   matrices    ref to an array of transformation-matrices. will be populated by this function.
 */
void build_node_matrices_bfs(const NodeConstPtr &root, const node_animation_t &animation, float time,
                             std::vector<vierkant::transform_t> &transforms);

/**
 * @brief   Create morph-weights, matching the provided node-hierarchy and animation.
 *
 * @tparam  T               scalar template type (float/double)
 * @param   root            a root node of a node-hierarchy.
 * @param   animation       a const-ref for an animation_t object.
 * @param   time            current time.
 * @param   morph_weights   ref to an array of morph-weights. will be populated by this function.
 */
template<typename T = float, typename = std::enable_if<std::is_floating_point_v<T>>>
void build_morph_weights_bfs(const NodeConstPtr &root, const node_animation_t &animation, float time,
                             std::vector<std::vector<T>> &morph_weights);

}// namespace vierkant::nodes
