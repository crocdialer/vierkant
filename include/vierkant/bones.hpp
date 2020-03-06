//
// Created by crocdialer on 2/21/20.
//

#pragma once

#include <memory>
#include <map>
#include <list>
#include <vierkant/math.hpp>

namespace vierkant::bones
{

using BonePtr = std::shared_ptr<struct bone_t>;
using BoneConstPtr = std::shared_ptr<const struct bone_t>;

struct bone_t
{
    std::string name;
    glm::mat4 transform = glm::mat4(1);
    glm::mat4 world_transform = glm::mat4(1);
    glm::mat4 offset = glm::mat4(1);
    uint32_t index = 0;
    BonePtr parent = nullptr;
    std::list<BonePtr> children;
};

template<typename T>
struct key_t
{
    float time = 0.f;
    T value = T(0);
};

struct animation_keys_t
{
    std::vector<key_t<glm::vec3>> position_keys;
    std::vector<key_t<glm::quat>> rotation_keys;
    std::vector<key_t<glm::vec3>> scale_keys;
};

struct animation_t
{
    float current_time = 0.f;
    float duration = 0.f;
    float ticks_per_sec = 0.f;
    std::map<BoneConstPtr, animation_keys_t> bone_keys;
};

// each vertex can reference up to 4 bones
struct vertex_data_t
{
    glm::ivec4 indices = glm::ivec4(0);
    glm::vec4 weights = glm::vec4(0);
};

/**
 * @brief   Return the total number of bones.
 *
 * @param   root    the root-bone of a hierarchy.
 *
 * @return  the total number of bones.
 */
uint32_t num_bones_in_hierarchy(const BoneConstPtr &root);

/**
 * @brief   Attempt to find a bone by name.
 *
 * @param   root    a root bone of a bone-hierarchy.
 *
 * @param   name    the name to search for.
 *
 * @return  the found BonePtr or nullptr, if the name could not be found in the hierarchy.
 */
BoneConstPtr bone_by_name(BoneConstPtr root, const std::string &name);

/**
 * @brief   Create transformation matrices, matching the provided bone-hierarchy and animation.
 *
 * @param   root        a root bone of a bone-hierarchy.
 *
 * @param   animation   a const-ref for an animation_t object.
 *
 * @param   matrices    ref to an array of transformation-matrices. will be recursively populated by this function.
 */
void build_bone_matrices(const BoneConstPtr& root, const animation_t &animation, std::vector<glm::mat4> &matrices);

}// namespace vierkant::bones
