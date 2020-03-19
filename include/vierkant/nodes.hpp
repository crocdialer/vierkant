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
    glm::mat4 world_transform = glm::mat4(1);
    glm::mat4 offset = glm::mat4(1);
    uint32_t index = 0;
    NodePtr parent = nullptr;
    std::list<NodePtr> children;
};

//! define a bone_animation type
using node_animation_t = vierkant::animation_t<NodeConstPtr>;

//! each vertex can reference up to 4 bones
struct vertex_data_t
{
    glm::ivec4 indices = glm::ivec4(0);
    glm::vec4 weights = glm::vec4(0);
};

/**
 * @brief   Return the total number of nodes.
 *
 * @param   root    the root-node of a hierarchy.
 *
 * @return  the total number of nodes.
 */
uint32_t num_nodes_in_hierarchy(const NodeConstPtr &root);

/**
 * @brief   Attempt to find a bone by name.
 *
 * @param   root    a root bone of a bone-hierarchy.
 *
 * @param   name    the name to search for.
 *
 * @return  the found BonePtr or nullptr, if the name could not be found in the hierarchy.
 */
NodeConstPtr node_by_name(NodeConstPtr root, const std::string &name);

/**
 * @brief   Create transformation matrices, matching the provided bone-hierarchy and animation.
 *
 * @param   root        a root bone of a bone-hierarchy.
 *
 * @param   animation   a const-ref for an animation_t object.
 *
 * @param   matrices    ref to an array of transformation-matrices. will be recursively populated by this function.
 */
void build_node_matrices(const NodeConstPtr& root, const node_animation_t &animation, std::vector<glm::mat4> &matrices);

}// namespace vierkant::bones
