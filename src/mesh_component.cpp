#include <stack>
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
                                                 static_cast<float>(anim_state->current_time),
                                                 anim_state->interpolation_mode, node_transforms);
    }

    auto add_entry_to_aabb = [&ret, &node_transforms](const Mesh::entry_t &entry, bool mesh_library) {
        if(mesh_library) { ret += entry.bounding_box; }
        else
        {
            ret += entry.bounding_box.transform(node_transforms.empty() ? entry.transform
                                                                        : node_transforms[entry.node_index]);
        }
    };

    if(cmp.entry_indices)
    {
        for(auto idx: *cmp.entry_indices) { add_entry_to_aabb(cmp.mesh->entries[idx], cmp.library); }
    }
    else
    {
        for(const auto &entry: cmp.mesh->entries) { add_entry_to_aabb(entry, cmp.library); }
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
                                                 static_cast<float>(anim_state->current_time),
                                                 anim_state->interpolation_mode, node_transforms);
    }

    auto add_aabb = [&ret, &node_transforms](const Mesh::entry_t &entry, bool mesh_library) {
        if(mesh_library) { ret.push_back(entry.bounding_box); }
        else
        {
            ret.push_back(entry.bounding_box.transform(node_transforms.empty() ? entry.transform
                                                                               : node_transforms[entry.node_index]));
        }
    };

    if(cmp.entry_indices)
    {
        for(auto idx: *cmp.entry_indices) { add_aabb(cmp.mesh->entries[idx], cmp.library); }
    }
    else
    {
        for(const auto &entry: cmp.mesh->entries) { add_aabb(entry, cmp.library); }
    }
    return ret;
}

Object3D *bone_mirror_root(const vierkant::Object3D &mesh_object)
{
    for(const auto &child: mesh_object.children)
    {
        if(child->has_component<vierkant::bone_component_t>()) { return child.get(); }
    }
    return nullptr;
}

Object3D *bone_object_by_id(const vierkant::Object3D &mesh_object, vierkant::nodes::NodeId node_id)
{
    auto *mirror_root = bone_mirror_root(mesh_object);
    if(!mirror_root || node_id.is_nil()) { return nullptr; }

    std::stack<vierkant::Object3D *> object_stack;
    object_stack.push(mirror_root);

    while(!object_stack.empty())
    {
        auto *object = object_stack.top();
        object_stack.pop();

        // stop at attached content, only the mirror itself is searched
        const auto *bone_cmp = object->get_component_ptr<vierkant::bone_component_t>();
        if(!bone_cmp) { continue; }
        if(bone_cmp->node_id == node_id) { return object; }

        for(const auto &child: object->children) { object_stack.push(child.get()); }
    }
    return nullptr;
}

}// namespace vierkant
