#include "vierkant/Scene.hpp"
#include "vierkant/Visitor.hpp"
#include "vierkant/cubemap_utils.hpp"

namespace vierkant
{

struct range_item_t
{
    Object3D *object = nullptr;
    float distance = 0.f;

    inline bool operator<(const range_item_t &other) const{ return distance < other.distance; }
};

vierkant::Object3DPtr create_mesh_object(const std::shared_ptr<entt::registry> &registry,
                                         const vierkant::MeshPtr &mesh)
{
    auto object = vierkant::Object3D::create(registry);
    object->add_component(mesh);
    vierkant::Object3D::aabb_fn_t aabb_fn;
    vierkant::Object3D::sub_aabb_fn_t sub_aabb_fn;
    if(!mesh->node_animations.empty()){ object->add_component<vierkant::animation_state_t>(); }

    aabb_fn = [mesh](const std::optional<vierkant::animation_state_t> &anim_state)
    {
        vierkant::AABB ret = {};

        // entry animation transforms
        std::vector<glm::mat4> node_matrices;

        if(!mesh->root_bone && anim_state && anim_state->index < mesh->node_animations.size())
        {
            const auto &animation = mesh->node_animations[anim_state->index];
            vierkant::nodes::build_node_matrices_bfs(mesh->root_node, animation,
                                                     anim_state->current_time,
                                                     node_matrices);
        }

        for(const auto &entry: mesh->entries)
        {
            ret += entry.bounding_box.transform(
                    (node_matrices.empty() ? entry.transform : node_matrices[entry.node_index]));
        }
        return ret;
    };
    object->add_component(aabb_fn);
    vierkant::AABB aabb = aabb_fn({});
    object->add_component(aabb);

    sub_aabb_fn = [mesh](const std::optional<vierkant::animation_state_t> &anim_state)
    {
        std::vector<vierkant::AABB> ret;

        // entry animation transforms
        std::vector<glm::mat4> node_matrices;

        if(!mesh->root_bone && anim_state && anim_state->index < mesh->node_animations.size())
        {
            const auto &animation = mesh->node_animations[anim_state->index];
            vierkant::nodes::build_node_matrices_bfs(mesh->root_node, animation,
                                                     anim_state->current_time,
                                                     node_matrices);
        }

        for(const auto &entry: mesh->entries)
        {
            ret.push_back(entry.bounding_box.transform(
                    (node_matrices.empty() ? entry.transform : node_matrices[entry.node_index])));
        }
        return ret;
    };
    object->add_component(sub_aabb_fn);
    return object;
}

ScenePtr Scene::create()
{
    return ScenePtr(new Scene());
}

void Scene::add_object(const Object3DPtr &object)
{
    m_root->add_child(object);
}

void Scene::remove_object(const Object3DPtr &object)
{
    m_root->remove_child(object, true);
}

void Scene::clear()
{
//    m_registry = std::make_shared<entt::registry>();
    m_root = vierkant::Object3D::create(m_registry, "scene root");
}

void Scene::update(double time_delta)
{
    auto mesh_animations_view = m_registry->view<animation_state_t, vierkant::MeshPtr>();

    for(const auto &[entity, animation_state, mesh]: mesh_animations_view.each())
    {
        vierkant::update_animation(mesh->node_animations[animation_state.index], time_delta, animation_state);
    }
}

Object3DPtr Scene::pick(const Ray &ray) const
{
    Object3DPtr ret;
    auto objects_view = m_registry->view<vierkant::Object3D *>();
    std::list<range_item_t> clicked_items;

    for(const auto &[entity, object]: objects_view.each())
    {
        if(object == m_root.get()){ continue; }

        auto obb = object->obb().transform(vierkant::mat4_cast(object->global_transform()));
        if(auto ray_hit = vierkant::intersect(obb, ray))
        {
            clicked_items.push_back({object, ray_hit.distance});
        }
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
    if(!img){ return; }
    m_skybox = img;
}

}//namespace
