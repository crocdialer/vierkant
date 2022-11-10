//
// Created by crocdialer on 30/4/19.
//

#pragma once

#include <list>
#include <crocore/crocore.hpp>
#include <entt/entity/registry.hpp>
#include <optional>

#include "vierkant/intersection.hpp"

namespace vierkant
{

//! forward declare Scene
DEFINE_CLASS_PTR(Scene);

DEFINE_CLASS_PTR(Object3D);

/**
 * @brief   Utility to check if one set of tags contains at least one item from another set.
 *
 * @param   whitelist the tags that shall pass the check.
 * @param   obj_tags    the tags to check against the whitelist
 * @return
 */
inline static bool check_tags(const std::set<std::string> &whitelist, const std::set<std::string> &obj_tags)
{
    for(const auto &t: obj_tags){ if(whitelist.count(t)){ return true; }}
    return whitelist.empty();
}

class Object3D : public std::enable_shared_from_this<Object3D>
{
public:

    using update_fn_t = std::function<void(float)>;

    static Object3DPtr create(const std::shared_ptr<entt::registry> &registry = {},
                              std::string name = "");

    virtual ~Object3D() noexcept = default;

    inline uint32_t id() const{ return m_id; };

    void add_tag(const std::string &tag, bool recursive = false);

    void remove_tag(const std::string &tag, bool recursive = false);

    void set_position(const glm::vec3 &pos);

    inline glm::vec3 position() const{ return transform[3].xyz(); }

    inline glm::vec3 lookAt() const{ return normalize(-transform[2].xyz()); }

    inline glm::vec3 side() const{ return normalize(transform[0].xyz()); }

    inline glm::vec3 up() const{ return normalize(transform[1].xyz()); }

    void set_rotation(const glm::quat &rot);

    void set_rotation(const glm::mat3 &rot);

    void set_rotation(float pitch, float yaw, float roll);

    glm::quat rotation() const;

    inline glm::vec3 scale()
    {
        return {length(transform[0]), length(transform[1]), length(transform[2])};
    };

    void set_scale(const glm::vec3 &s);

    inline void set_scale(float s){ set_scale(glm::vec3(s)); }

    void set_look_at(const glm::vec3 &lookAt, const glm::vec3 &up = glm::vec3(0, 1, 0));

    void set_look_at(const Object3DPtr &lookAt);

    void set_parent(const Object3DPtr &parent);

    inline Object3DPtr parent() const{ return m_parent.lock(); }

    void add_child(const Object3DPtr &child);

    void remove_child(const Object3DPtr &child, bool recursive = false);

    glm::mat4 global_transform() const;

    glm::vec3 global_position() const;

    glm::quat global_rotation() const;

    glm::vec3 global_scale() const;

    void set_global_transform(const glm::mat4 &transform);

    void set_global_position(const glm::vec3 &position);

    void set_global_rotation(const glm::quat &rotation);

    void set_global_scale(const glm::vec3 &scale);

    /**
     * @return the axis-aligned boundingbox (AABB) in object coords.
     */
    virtual AABB aabb() const;

    virtual OBB obb() const;

    /*!
     * Performs an update on this scenegraph node.
     * Triggered by gl::Scene instances during gl::Scene::update(float delta) calls
     */
    virtual void update(float time_delta)
    {
        if(m_update_function){ m_update_function(time_delta); }
    };

    /*!
     * Provide a function object to be called on each update
     */
    void set_update_function(update_fn_t f){ m_update_function = f; }

    virtual void accept(class Visitor &theVisitor);

    template<typename T>
    void add_component(const T &component = {})
    {
        if(auto reg = m_registry.lock())
        {
            reg->template emplace<T>(m_entity, component);
        }
    }

    template<typename T>
    bool has_component() const { return get_component_ptr<T>(); }

    template<typename T>
    T* get_component_ptr()
    {
        auto reg = m_registry.lock();
        return reg ? reg->try_get<T>(m_entity) : nullptr;
    }

    template<typename T>
    const T* get_component_ptr() const
    {
        auto reg = m_registry.lock();
        return reg ? reg->try_get<T>(m_entity) : nullptr;
    }

    template<typename T>
    T &get_component()
    {
        auto ptr = get_component_ptr<T>();
        if(ptr){ return *ptr; }
        throw std::runtime_error("component does not exist");
    }

    template<typename T>
    inline const T &get_component() const
    {
        auto ptr = get_component_ptr<T>();
        if(ptr){ return *ptr; }
        throw std::runtime_error("component does not exist");
    }

    //! set of tags
    std::set<std::string> tags;

    //! user definable name
    std::string name;

    //! enabled hint, can be used by Visitors
    bool enabled;

    //! billboard hint, can be used by Visitors
    bool billboard;

    //! a transformation-matrix for this object
    glm::mat4 transform = glm::mat4(1);

    //! a list of child-objects
    std::list<Object3DPtr> children;

protected:
    explicit Object3D(const std::shared_ptr<entt::registry>& registry,
                      std::string name = "");

private:

    static uint32_t s_id_pool;

    //! unique id
    uint32_t m_id;

    std::weak_ptr<Object3D> m_parent;

    update_fn_t m_update_function;

    std::weak_ptr<entt::registry> m_registry;

    entt::entity m_entity;
};

}//namespace
