#pragma once

#include <optional>
#include <unordered_set>
#include <vector>
#include <vierkant/Mesh.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

struct mesh_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! handle to a mesh, containing buffers and a list of entries
    vierkant::MeshConstPtr mesh;

    //! optional set of used entry-indices.
    std::optional<std::unordered_set<uint32_t>> entry_indices = {};

    //! flag indicating that the mesh is used as mesh-library and entry-transforms should be skipped
    bool library = false;
};

//! struct grouping host/gpu versions of a mesh
struct mesh_asset_t
{
    //! handle for a gpu-mesh, containing buffers and a list of entries
    vierkant::MeshPtr mesh;

    //! optional, persist-able bundle-version
    std::optional<vierkant::mesh_buffer_bundle_t> bundle;
};
using mesh_map_t = std::unordered_map<vierkant::MeshId, mesh_asset_t>;

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
