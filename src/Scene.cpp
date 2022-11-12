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
    m_registry = std::make_shared<entt::registry>();
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

Object3DPtr Scene::pick(const Ray &ray, bool high_precision,
                        const std::set<std::string> &tags) const
{
    Object3DPtr ret;
    auto objects_view = m_registry->view<vierkant::Object3D*>();
    std::list<range_item_t> clicked_items;

    for(const auto &[entity, object] : objects_view.each())
    {
        if(object == m_root.get()){ continue; }

        if(auto ray_hit = vierkant::intersect(object->obb().transform(object->global_transform()), ray))
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
