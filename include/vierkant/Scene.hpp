#pragma once

#include <entt/entity/registry.hpp>

#include <vierkant/Object3D.hpp>
#include <vierkant/Camera.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/Mesh.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Scene)

using double_second = std::chrono::duration<double>;


/**
 * @brief   'create_mesh_object' is a factory to create an Object3D containing a mesh.
 *
 * in addition the created object offers support for animations and dynamically updated aabbs for submeshes.
 *
 * @param   registry    a provided entt::registry
 * @param   mesh        a provided mesh
 * @return  a newly created Object3D with attached components.
 */
vierkant::Object3DPtr create_mesh_object(const std::shared_ptr<entt::registry> &registry,
                                         const mesh_component_t &mesh_component);

class Scene
{
public:

    static ScenePtr create();

    void update(double time_delta);

    /**
     * @brief   object_by_id finds and returns an object based on its object/entity-id
     *
     * @param   object_id   a provided object-id
     * @return  an object or nullptr, if nothing was found
     */
    [[nodiscard]] Object3DPtr object_by_id(uint32_t object_id) const;

    [[nodiscard]] Object3DPtr pick(const Ray &ray) const;

    void add_object(const Object3DPtr &object);

    void remove_object(const Object3DPtr &object);

    void clear();

    [[nodiscard]] inline const Object3DPtr &root() const{ return m_root; };

    [[nodiscard]] const vierkant::ImagePtr &environment() const{ return m_skybox; }

    void set_environment(const vierkant::ImagePtr &img);

    [[nodiscard]] const std::shared_ptr<entt::registry>& registry() const { return m_registry; }

private:

    Scene() = default;

    std::shared_ptr<entt::registry> m_registry = std::make_shared<entt::registry>();

    vierkant::ImagePtr m_skybox = nullptr;

    Object3DPtr m_root = Object3D::create(m_registry, "scene root");

    //! material-store
    std::unordered_map<vierkant::MaterialId, material_t> m_materials;

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
};

}//namespace
