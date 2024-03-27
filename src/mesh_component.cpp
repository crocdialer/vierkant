#include <vierkant/mesh_component.hpp>

namespace vierkant
{
AABB mesh_aabb(const vierkant::mesh_component_t &cmp, const std::optional<vierkant::animation_component_t> &anim_state)
{
    vierkant::AABB ret = {};

    // entry animation transforms
    std::vector<vierkant::transform_t> node_transforms;

    if(!cmp.mesh->root_bone && anim_state && anim_state->index < cmp.mesh->node_animations.size())
    {
        const auto &animation = cmp.mesh->node_animations[anim_state->index];
        vierkant::nodes::build_node_matrices_bfs(cmp.mesh->root_node, animation,
                                                 static_cast<float>(anim_state->current_time), node_transforms);
    }

    auto add_entry_to_aabb = [&ret, &node_transforms](const Mesh::entry_t &entry) {
        ret += entry.bounding_box.transform(node_transforms.empty() ? entry.transform
                                                                    : node_transforms[entry.node_index]);
    };

    if(cmp.entry_indices)
    {
        for(auto idx: *cmp.entry_indices) { add_entry_to_aabb(cmp.mesh->entries[idx]); }
    }
    else
    {
        for(const auto &entry: cmp.mesh->entries) { add_entry_to_aabb(entry); }
    }
    return ret;
}

std::vector<vierkant::AABB> mesh_sub_aabbs(const vierkant::mesh_component_t &cmp,
                                           const std::optional<vierkant::animation_component_t> &anim_state)
{
    std::vector<vierkant::AABB> ret;

    // entry animation transforms
    std::vector<vierkant::transform_t> node_transforms;

    if(!cmp.mesh->root_bone && anim_state && anim_state->index < cmp.mesh->node_animations.size())
    {
        const auto &animation = cmp.mesh->node_animations[anim_state->index];
        vierkant::nodes::build_node_matrices_bfs(cmp.mesh->root_node, animation,
                                                 static_cast<float>(anim_state->current_time), node_transforms);
    }

    auto add_aabb = [&ret, &node_transforms](const Mesh::entry_t &entry) {
        ret.push_back(entry.bounding_box.transform(node_transforms.empty() ? entry.transform
                                                                           : node_transforms[entry.node_index]));
    };

    if(cmp.entry_indices)
    {
        for(auto idx: *cmp.entry_indices) { add_aabb(cmp.mesh->entries[idx]); }
    }
    else
    {
        for(const auto &entry: cmp.mesh->entries) { add_aabb(entry); }
    }
    return ret;
}
}
