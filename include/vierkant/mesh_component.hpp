#pragma once

#include <vector>
#include <set>
#include <optional>
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
    std::optional<std::set<uint32_t>> entry_indices = {};
};

AABB mesh_aabb(const vierkant::mesh_component_t &cmp, const std::optional<vierkant::animation_component_t> &anim_state);

std::vector<vierkant::AABB> mesh_sub_aabbs(const vierkant::mesh_component_t &cmp,
                                           const std::optional<vierkant::animation_component_t> &anim_state);

}
