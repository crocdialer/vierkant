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

DEFINE_CLASS_PTR(Object3D);

class Object3D : public std::enable_shared_from_this<Object3D>
{
public:

    using update_fn_t = std::function<void(float)>;

    static Object3DPtr create(const std::shared_ptr<entt::registry> &registry = {},
                              std::string name = "");

    virtual ~Object3D() noexcept;

    inline uint32_t id() const{ return static_cast<uint32_t>(m_entity); };

    void add_tag(const std::string &tag, bool recursive = false);

    void remove_tag(const std::string &tag, bool recursive = false);

    inline Object3DPtr parent() const{ return m_parent.lock(); }

    void add_child(const Object3DPtr &child);

    void remove_child(const Object3DPtr &child, bool recursive = false);

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
    virtual AABB aabb() const{ auto aabb_ptr = get_component_ptr<vierkant::AABB>(); return aabb_ptr ? * aabb_ptr : AABB(); };

    virtual OBB obb() const;

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
    inline const T* get_component_ptr() const
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

    //! a transformation-matrix for this object
    glm::mat4 transform = glm::mat4(1);

    //! a list of child-objects
    std::list<Object3DPtr> children;

protected:
    explicit Object3D(const std::shared_ptr<entt::registry>& registry,
                      std::string name = "");

private:

    std::weak_ptr<Object3D> m_parent;

    std::weak_ptr<entt::registry> m_registry;

    entt::entity m_entity;
};

}//namespace
