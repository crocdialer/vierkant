#pragma once

#include <optional>
#include <set>
#include <vector>
#include <vierkant/Mesh.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

DEFINE_NAMED_UUID(MeshId)

struct mesh_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! handle to a mesh, containing buffers and a list of entries
    vierkant::MeshConstPtr mesh;

    //! optional set of used entry-indices.
    std::optional<std::set<uint32_t>> entry_indices = {};
};

/**
 * @brief   mesh_aabb can be used to generate a combined AABB for all activated mesh-entries,
 *          optionally applying animation-transforms.
 *
 * @param   cmp         a provided vierkant::mesh_component_t
 * @param   anim_state  optional animation-state
 * @return  a combined AABB
 */
AABB mesh_aabb(const vierkant::mesh_component_t &cmp, const std::optional<vierkant::animation_component_t> &anim_state);

/**
 * @brief   mesh_sub_aabbs can be used to generate a sequence of sub-AABBs for all activated mesh-entries,
 *          optionally applying animation-transforms.
 *
 * @param   cmp         a provided vierkant::mesh_component_t
 * @param   anim_state  optional animation-state
 * @return  a sequence containing sub-AABBs for active mesh-entries
 */
std::vector<vierkant::AABB> mesh_sub_aabbs(const vierkant::mesh_component_t &cmp,
                                           const std::optional<vierkant::animation_component_t> &anim_state);

}// namespace vierkant
