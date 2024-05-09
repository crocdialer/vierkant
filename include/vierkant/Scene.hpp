#pragma once

#include <entt/entity/registry.hpp>

#include <crocore/ThreadPool.hpp>

#include <vierkant/Camera.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Object3D.hpp>
#include <vierkant/mesh_component.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Scene)


class Scene
{
public:
    virtual ~Scene() = default;

    static ScenePtr create();

    virtual void add_object(const Object3DPtr &object);

    virtual void remove_object(const Object3DPtr &object);

    virtual void clear();

    virtual void update(double time_delta);

    /**
     * @brief   object_by_id finds and returns an object based on its object/entity-id
     *
     * @param   object_id   a provided object-id
     * @return  an object or nullptr, if nothing was found
     */
    [[nodiscard]] Object3D *object_by_id(uint32_t object_id) const;

    [[nodiscard]] std::vector<Object3D *> objects_by_name(const std::string_view &name) const;

    [[nodiscard]] Object3DPtr pick(const Ray &ray) const;

    [[nodiscard]] inline const Object3DPtr &root() const { return m_root; };

    [[nodiscard]] const vierkant::ImagePtr &environment() const { return m_skybox; }

    void set_environment(const vierkant::ImagePtr &img);

    [[nodiscard]] const std::shared_ptr<entt::registry> &registry() const { return m_registry; }

    /**
    * @brief   'create_mesh_object' is a factory to create an Object3D containing a mesh.
    *
    * in addition the created object offers support for animations and dynamically updated aabbs for submeshes.
    *
    * @param   registry    a provided entt::registry
    * @param   mesh        a provided mesh
    * @return  a newly created Object3D with attached components.
    */
    vierkant::Object3DPtr create_mesh_object(const vierkant::mesh_component_t &mesh_component);

protected:
    explicit Scene() = default;

private:
    std::shared_ptr<entt::registry> m_registry = std::make_shared<entt::registry>();

    vierkant::ImagePtr m_skybox = nullptr;

    Object3DPtr m_root = Object3D::create(m_registry, "scene root");

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
};

//! helper struct to group an entity/id with a sub-entry-index
struct id_entry_t
{
    uint32_t id;
    uint32_t entry;

    inline bool operator==(const id_entry_t &other) const { return id == other.id && entry == other.entry; }
};

}// namespace vierkant

namespace std
{
template<>
struct hash<vierkant::id_entry_t>
{
    size_t operator()(vierkant::id_entry_t const &key) const;
};

}// namespace std
