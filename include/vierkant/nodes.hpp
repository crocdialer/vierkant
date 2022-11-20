//
// Created by crocdialer on 2/21/20.
//

#pragma once

#include <memory>
#include <map>
#include <list>
#include <vierkant/math.hpp>
#include <vierkant/animation.hpp>

namespace vierkant::nodes
{

using NodePtr = std::shared_ptr<struct node_t>;
using NodeConstPtr = std::shared_ptr<const struct node_t>;

struct node_t
{
    std::string name;
    glm::mat4 transform = glm::mat4(1);
    glm::mat4 offset = glm::mat4(1);
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
 *
 * @return  the total number of nodes.
 */
uint32_t num_nodes_in_hierarchy(const NodeConstPtr &root);

/**
 * @brief   Attempt to find a node by name.
 *
 * @param   root    a root bone of a node-hierarchy.
 *
 * @param   name    the name to search for.
 *
 * @return  the found NodePtr or nullptr, if the name could not be found in the hierarchy.
 */
NodeConstPtr node_by_name(NodeConstPtr root, const std::string &name);

/**
 * @brief   Create transformation matrices, matching the provided node-hierarchy and animation.
 *
 * @param   root        a root node of a node-hierarchy.
 *
 * @param   animation   a const-ref for an animation_t object.
 *
 * @param   matrices    ref to an array of transformation-matrices. will be recursively populated by this function.
 */
void build_node_matrices(const NodeConstPtr &root,
                         const node_animation_t &animation,
                         double animation_time,
                         std::vector<glm::mat4> &matrices);

void build_node_matrices_bfs(const NodeConstPtr &root,
                             const node_animation_t &animation,
                             double animation_time,
                             std::vector<glm::mat4> &matrices);

void build_morph_weights_bfs(const NodeConstPtr &root,
                             const node_animation_t &animation,
                             double animation_time,
                             std::vector<std::vector<float>> &morph_weights);

}// namespace vierkant::bones
