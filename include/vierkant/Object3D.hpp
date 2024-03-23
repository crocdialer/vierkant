//
// Created by crocdialer on 30/4/19.
//

#pragma once

#include <list>
#include <optional>
#include <set>

#include <crocore/crocore.hpp>
#include <entt/entity/registry.hpp>
#include <vierkant/animation.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/object_component.hpp>
#include <vierkant/transform.hpp>

namespace vierkant
{

struct aabb_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! signature for a function to retrieve a combined AABB
    using aabb_fn_t = std::function<vierkant::AABB(const std::optional<vierkant::animation_component_t> &)>;

    //! signature for a function to retrieve all sub-AABBs
    using sub_aabb_fn_t =
            std::function<std::vector<vierkant::AABB>(const std::optional<vierkant::animation_component_t> &)>;

    vierkant::AABB aabb;
    aabb_fn_t aabb_fn;
    sub_aabb_fn_t sub_aabb_fn;
};

DEFINE_CLASS_PTR(Object3D)

class Object3D : public std::enable_shared_from_this<Object3D>
{
public:
    static Object3DPtr create(const std::shared_ptr<entt::registry> &registry, std::string name = "");

    virtual ~Object3D() noexcept;

    inline uint32_t id() const { return static_cast<uint32_t>(m_entity); };

    inline Object3DPtr parent() const { return m_parent.lock(); }

    void add_child(const Object3DPtr &child);

    void remove_child(const Object3DPtr &child, bool recursive = false);

    void set_parent(const Object3DPtr &parent);

    vierkant::transform_t global_transform() const;

    void set_global_transform(const vierkant::transform_t &t);

    /**
     * @return the axis-aligned boundingbox (AABB) in object coords.
     */
    AABB aabb() const;

    OBB obb() const;

    std::vector<AABB> sub_aabbs() const;

    virtual void accept(class Visitor &theVisitor);

    /**
     * @brief   'add_component' can be used to attach an arbitrary component to this object's entity.
     *
     * @tparam  T           type of the component
     * @param   component   optional arbitrary component. will be copied if provided, otherwise default-constructed
     * @return  a reference for the newly created component.
     */
    template<object_component T>
    inline T &add_component(const T &component = {})
    {
        if(auto reg = m_registry.lock()) { return reg->template emplace_or_replace<T>(m_entity, component); }
        throw std::runtime_error("error adding component: no registry defined");
    }

    /**
     * @brief   'has_component' can be used to determine if a given type exists among an object's components.
     *
     * @tparam  T   type of the component
     * @return  true, if a component of the provided type exists.
     */
    template<object_component T>
    inline bool has_component() const
    {
        return get_component_ptr<T>();
    }

    /**
     * @brief   'remove_component' removes existing components.
     *
     * @tparam  T   type of the component
     * @return  true, if a component of the provided type existed.
     */
    template<object_component T>
    inline bool remove_component()
    {
        auto reg = m_registry.lock();
        if(reg && reg->try_get<T>(m_entity))
        {
            reg->remove<T>(m_entity);
            return true;
        }
        return false;
    }

    /**
     * @brief   'get_component_ptr' can be used to retrieve a pointer to an object's component, if existing.
     *
     * @tparam  T   type of the component
     * @return  a pointer to a component of the provided type, if available. nullptr otherwise.
     */
    template<object_component T>
    inline T *get_component_ptr()
    {
        auto reg = m_registry.lock();
        return reg ? reg->try_get<T>(m_entity) : nullptr;
    }

    /**
     * @brief   'get_component_ptr' can be used to retrieve a pointer to an object's component, if existing.
     *
     * @tparam  T   type of the component
     * @return  a pointer to a component of the provided type, if available. nullptr otherwise.
     */
    template<object_component T>
    inline const T *get_component_ptr() const
    {
        auto reg = m_registry.lock();
        return reg ? reg->try_get<T>(m_entity) : nullptr;
    }

    /**
     * @brief   'get_component' can be used to retrieve a reference to an object's component, if existing.
     *          will throw if no object of the requested type could be found.
     *
     * @tparam  T   type of the component
     * @return  a reference to an associated component of the provided type.
     */
    template<object_component T>
    inline T &get_component()
    {
        auto ptr = get_component_ptr<T>();
        if(ptr) { return *ptr; }
        throw std::runtime_error("component does not exist");
    }

    /**
     * @brief   get_component can be used to retrieve a reference to an object's component, if existing.
     *          will throw if no object of the requested type could be found.
     *
     * @tparam  T   type of the component
     * @return  a reference to an associated component of the provided type.
     */
    template<object_component T>
    inline const T &get_component() const
    {
        auto ptr = get_component_ptr<T>();
        if(ptr) { return *ptr; }
        throw std::runtime_error("component does not exist");
    }

    /**
     * @brief   clone will perform a recursive deep-copy, including all components.
     *
     * @return  a newly created Object3DPtr, containing a deep-copy of entire sub-tree
     */
    Object3DPtr clone() const;

    //! set of tags
    std::set<std::string> tags;

    //! user definable name
    std::string name;

    //! enabled hint, can be used by Visitors
    bool enabled = true;

    //! the transformation of this object
    vierkant::transform_t transform = {};

    //! a list of child-objects
    std::list<Object3DPtr> children;

    VIERKANT_ENABLE_AS_COMPONENT();

protected:
    explicit Object3D(const std::shared_ptr<entt::registry> &registry, std::string name = "");

private:
    std::weak_ptr<Object3D> m_parent;

    std::weak_ptr<entt::registry> m_registry;

    entt::entity m_entity;
};

}// namespace vierkant
