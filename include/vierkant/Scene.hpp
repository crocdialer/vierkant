#pragma once

#include <entt/entity/registry.hpp>

#include <vierkant/Object3D.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Camera.hpp>
#include <vierkant/Image.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Scene);

using double_second = std::chrono::duration<double>;

class Scene
{
public:

    static ScenePtr create();

    void update(double time_delta);

    [[nodiscard]] Object3DPtr pick(const Ray &ray, bool high_precision = false,
                                   const std::set<std::string> &tags = {}) const;

    void add_object(const Object3DPtr &object);

    void remove_object(const Object3DPtr &object);

    void clear();

    [[nodiscard]] vierkant::Object3DPtr object_by_name(const std::string &name) const;

    [[nodiscard]] std::vector<vierkant::Object3DPtr> objects_by_tags(const std::set<std::string> &tags) const;

    [[nodiscard]] std::vector<vierkant::Object3DPtr> objects_by_tag(const std::string &tag) const;

    [[nodiscard]] inline const Object3DPtr &root() const{ return m_root; };

    [[nodiscard]] const vierkant::ImagePtr &environment() const{ return m_skybox; }

    void set_environment(const vierkant::ImagePtr &img);

    [[nodiscard]] const std::shared_ptr<entt::registry>& registry() const { return m_registry; }

private:

    Scene();

    std::shared_ptr<entt::registry> m_registry = std::make_shared<entt::registry>();

//    std::unordered_map<Object3DPtr, entt::entity> m_entities;

    vierkant::ImagePtr m_skybox = nullptr;

    Object3DPtr m_root = Object3D::create(nullptr, "scene root");

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
    vierkant::animation_state_t m_animation_state = {};
};

}//namespace
