#include "vierkant/Scene.hpp"
#include "vierkant/Visitor.hpp"
#include "vierkant/physics_context.hpp"

namespace vierkant
{

struct range_item_t
{
    Object3D *object = nullptr;
    float distance = 0.f;

    inline bool operator<(const range_item_t &other) const { return distance < other.distance; }
};

vierkant::Object3DPtr Scene::create_mesh_object(const mesh_component_t &mesh_component)
{
    auto object = m_object_store->create_object();
    object->add_component(mesh_component);
    if(!mesh_component.mesh->node_animations.empty()) { object->add_component<vierkant::animation_component_t>(); }

    vierkant::object_component auto &aabb_component = object->add_component<vierkant::aabb_component_t>();

    aabb_component.aabb_fn = [](const vierkant::Object3D &obj) {
        AABB ret;
        if(const auto *mesh_cmp = obj.get_component_ptr<mesh_component_t>())
        {
            std::optional<vierkant::animation_component_t> anim_cmp;
            if(obj.has_component<animation_component_t>()) { anim_cmp = obj.get_component<animation_component_t>(); }
            ret = mesh_aabb(*mesh_cmp, anim_cmp);
        }
        return ret;
    };

    aabb_component.sub_aabb_fn = [](const vierkant::Object3D &obj) -> std::vector<vierkant::AABB> {
        if(const auto *mesh_cmp = obj.get_component_ptr<mesh_component_t>())
        {
            std::optional<vierkant::animation_component_t> anim_cmp;
            if(obj.has_component<animation_component_t>()) { anim_cmp = obj.get_component<animation_component_t>(); }
            return mesh_sub_aabbs(*mesh_cmp, anim_cmp);
        }
        return {};
    };
    return object;
}


Scene::Scene(const std::shared_ptr<vierkant::ObjectStore> &object_store)
    : m_object_store(object_store ? object_store : create_object_store())
{
    m_root = m_object_store->create_object();
    m_root->name = s_scene_root_name;
}

ScenePtr Scene::create(const std::shared_ptr<vierkant::ObjectStore> &object_store)
{
    return ScenePtr(new Scene(object_store));
}

void Scene::add_object(const Object3DPtr &object) { m_root->add_child(object); }

void Scene::remove_object(const Object3DPtr &object) { m_root->remove_child(object, true); }

void Scene::clear()
{
    m_root = m_object_store->create_object();
    m_root->name = s_scene_root_name;
}

void Scene::update(double time_delta)
{
    LambdaVisitor visitor;
    visitor.traverse(*m_root, [time_delta](Object3D &obj) -> bool {
        if(obj.enabled)
        {
            auto animation_cmp = obj.get_component_ptr<animation_component_t>();
            auto mesh_cmp = obj.get_component_ptr<mesh_component_t>();

            if(auto *flag_cmp = obj.get_component_ptr<flag_component_t>())
            {
                // clear previous dirt flags
                flag_cmp->flags &= ~flag_component_t::DIRTY_TRANSFORM;
            }
            if(animation_cmp && mesh_cmp)
            {
                vierkant::update_animation(mesh_cmp->mesh->node_animations[animation_cmp->index], time_delta,
                                           *animation_cmp);
            }
            if(auto *update_cmp = obj.get_component_ptr<update_component_t>())
            {
                if(update_cmp->update_fn) { update_cmp->update_fn(obj, time_delta); }
            }
            if(auto *timer_cmp = obj.get_component_ptr<timer_component_t>())
            {
                timer_cmp->duration -= timer_component_t::duration_t(time_delta);
                if(timer_cmp->duration <= timer_component_t::duration_t(0) && timer_cmp->timer_fn)
                {
                    timer_cmp->timer_fn(obj);

                    if(timer_cmp->repeat) { timer_cmp->duration += timer_cmp->total; }
                    else { timer_cmp->timer_fn = {}; }
                }
            }
            return true;
        }
        return false;
    });
}

Object3D *Scene::object_by_id(uint32_t object_id) const
{
    auto object_ptr = registry()->try_get<vierkant::Object3D *>(entt::entity(object_id));
    return object_ptr ? *object_ptr : nullptr;
}

std::vector<Object3D *> Scene::objects_by_name(const std::string_view &name) const
{
    std::vector<Object3D *> ret;
    auto view = registry()->view<Object3D *>();
    for(const auto &[entity, object]: view.each())
    {
        if(object->name == name) { ret.push_back(object); }
    }
    return ret;
}

Object3D *Scene::any_object_by_name(const std::string_view &name) const
{
    auto view = registry()->view<Object3D *>();
    for(const auto &[entity, object]: view.each())
    {
        if(object->name.find(name) != std::string::npos) { return object; }
    }
    return nullptr;
}

Object3DPtr Scene::pick(const Ray &ray) const
{
    Object3DPtr ret;
    auto objects_view = registry()->view<vierkant::Object3D *>();
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

size_t std::hash<vierkant::id_entry_t>::operator()(vierkant::id_entry_t const &key) const
{
    size_t h = 0;
    vierkant::hash_combine(h, key.id);
    vierkant::hash_combine(h, key.entry);
    return h;
}
