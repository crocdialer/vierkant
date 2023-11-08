#include "vierkant/Scene.hpp"
#include "vierkant/Visitor.hpp"
#include "vierkant/cubemap_utils.hpp"

namespace vierkant
{

struct range_item_t
{
    Object3D *object = nullptr;
    float distance = 0.f;

    inline bool operator<(const range_item_t &other) const { return distance < other.distance; }
};

vierkant::Object3DPtr create_mesh_object(const std::shared_ptr<entt::registry> &registry,
                                         const mesh_component_t &mesh_component)
{
    auto object = vierkant::Object3D::create(registry);
    object->add_component(mesh_component);
    if(!mesh_component.mesh->node_animations.empty()) { object->add_component<vierkant::animation_state_t>(); }

    auto weak_obj = vierkant::Object3DWeakPtr(object);
    auto &aabb_component = object->add_component<vierkant::aabb_component_t>();

    aabb_component.aabb_fn = [weak_obj](const std::optional<vierkant::animation_state_t> &anim_state) {
        vierkant::AABB ret = {};
        auto obj = weak_obj.lock();

        if(obj && obj->has_component<mesh_component_t>())
        {
            const auto &cmp = obj->get_component<mesh_component_t>();

            // entry animation transforms
            std::vector<vierkant::transform_t> node_transforms;

            if(!cmp.mesh->root_bone && anim_state && anim_state->index < cmp.mesh->node_animations.size())
            {
                const auto &animation = cmp.mesh->node_animations[anim_state->index];
                vierkant::nodes::build_node_matrices_bfs(cmp.mesh->root_node, animation,
                                                         static_cast<float>(anim_state->current_time), node_transforms);
            }

            auto add_entry_to_aabb = [&ret, &node_transforms](const Mesh::entry_t &entry) {
                ret += entry.bounding_box.transform(
                        mat4_cast(node_transforms.empty() ? entry.transform : node_transforms[entry.node_index]));
            };

            if(cmp.entry_indices)
            {
                for(auto idx: *cmp.entry_indices) { add_entry_to_aabb(cmp.mesh->entries[idx]); }
            }
            else
            {
                for(const auto &entry: cmp.mesh->entries) { add_entry_to_aabb(entry); }
            }
        }
        return ret;
    };

    aabb_component.sub_aabb_fn = [weak_obj](const std::optional<vierkant::animation_state_t> &anim_state) {
        std::vector<vierkant::AABB> ret;

        auto obj = weak_obj.lock();

        if(obj && obj->has_component<mesh_component_t>())
        {
            const auto &cmp = obj->get_component<mesh_component_t>();

            // entry animation transforms
            std::vector<vierkant::transform_t> node_transforms;

            if(!cmp.mesh->root_bone && anim_state && anim_state->index < cmp.mesh->node_animations.size())
            {
                const auto &animation = cmp.mesh->node_animations[anim_state->index];
                vierkant::nodes::build_node_matrices_bfs(cmp.mesh->root_node, animation,
                                                         static_cast<float>(anim_state->current_time), node_transforms);
            }

            auto add_aabb = [&ret, &node_transforms](const Mesh::entry_t &entry) {
                ret.push_back(entry.bounding_box.transform(
                        mat4_cast(node_transforms.empty() ? entry.transform : node_transforms[entry.node_index])));
            };

            if(cmp.entry_indices)
            {
                for(auto idx: *cmp.entry_indices) { add_aabb(cmp.mesh->entries[idx]); }
            }
            else
            {
                for(const auto &entry: cmp.mesh->entries) { add_aabb(entry); }
            }
        }
        return ret;
    };
    return object;
}

ScenePtr Scene::create() { return ScenePtr(new Scene()); }

void Scene::add_object(const Object3DPtr &object) { m_root->add_child(object); }

void Scene::remove_object(const Object3DPtr &object) { m_root->remove_child(object, true); }

void Scene::clear() { m_root = vierkant::Object3D::create(m_registry, "scene root"); }

void Scene::update(double time_delta)
{
    auto mesh_animations_view = m_registry->view<animation_state_t, vierkant::mesh_component_t>();

    for(const auto &[entity, animation_state, mesh_component]: mesh_animations_view.each())
    {
        vierkant::update_animation(mesh_component.mesh->node_animations[animation_state.index], time_delta,
                                   animation_state);
    }
}

Object3DPtr Scene::pick(const Ray &ray) const
{
    Object3DPtr ret;
    auto objects_view = m_registry->view<vierkant::Object3D *>();
    std::list<range_item_t> clicked_items;

    for(const auto &[entity, object]: objects_view.each())
    {
        if(object == m_root.get()) { continue; }

        auto obb = object->obb().transform(vierkant::mat4_cast(object->global_transform()));
        if(auto ray_hit = vierkant::intersect(obb, ray)) { clicked_items.push_back({object, ray_hit.distance}); }
    }
    if(!clicked_items.empty())
    {
        clicked_items.sort();
        ret = clicked_items.front().object->shared_from_this();
        spdlog::trace("ray hit id {} ({} total)", ret->id(), clicked_items.size());
    }
    return ret;
}

void Scene::set_environment(const vierkant::ImagePtr &img)
{
    if(!img) { return; }
    m_skybox = img;
}

}// namespace vierkant
