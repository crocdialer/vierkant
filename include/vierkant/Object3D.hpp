//
// Created by crocdialer on 30/4/19.
//

#pragma once

#include <list>
#include <optional>
#include <set>

#include <crocore/crocore.hpp>
#include <crocore/fixed_size_free_list.h>
#include <entt/entity/registry.hpp>
#include <vierkant/animation.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/object_component.hpp>
#include <vierkant/transform.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Object3D)

//! ObjectStore is responsible to create objects and connect to the entity-component-system
class ObjectStore
{
public:
    [[nodiscard]] virtual const std::shared_ptr<entt::registry> &registry() const = 0;

    /**
     * @return  a newly created Object3D via shared_ptr
     */
    virtual Object3DPtr create_object() = 0;

    /**
     * @brief   clone will perform a recursive deep-copy, including all components.
     *
     * @return  a newly created Object3DPtr, containing a deep-copy of entire sub-tree
     */
    virtual Object3DPtr clone(const vierkant::Object3D *object) = 0;
};

/**
 * @brief   create_object_store creates a new ObjectStore instance.
 *
 * @param   max_num_objects maximum number of objects that can be allocated from the store.
 * @param   page_size       number of objects per allocation-page
 * @return  a new ObjectStore instance
 */
std::unique_ptr<ObjectStore> create_object_store(uint32_t max_num_objects = 1 << 16, uint32_t page_size = 1 << 10);

struct aabb_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! signature for a function to retrieve a combined AABB
    using aabb_fn_t = std::function<vierkant::AABB(const vierkant::Object3D &)>;

    //! signature for a function to retrieve all sub-AABBs
    using sub_aabb_fn_t = std::function<std::vector<vierkant::AABB>(const vierkant::Object3D &)>;

    aabb_fn_t aabb_fn;
    sub_aabb_fn_t sub_aabb_fn;
};

struct update_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    //! signature for a function to update an object
    using update_fn_t = std::function<void(const vierkant::Object3D &obj, double delta)>;

    update_fn_t update_fn;
};

struct timer_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();
    using duration_t = std::chrono::duration<double, std::chrono::seconds::period>;

    //! signature for a function to update an object
    using timer_fn_t = std::function<void(const vierkant::Object3D &obj)>;

    duration_t duration, total;
    timer_fn_t timer_fn;
    bool repeat = false;
};

struct flag_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();
    enum FlagEnum
    {
        DIRTY_TRANSFORM = 1
    };
    uint32_t flags = 0;
};

bool has_inherited_flag(const vierkant::Object3D *object, uint32_t flag_bits);

class alignas(8) Object3D : public std::enable_shared_from_this<Object3D>
{
public:
    virtual ~Object3D() noexcept;

    inline uint32_t id() const { return static_cast<uint32_t>(m_entity); };

    inline Object3D *parent() const { return m_parent; }

    void add_child(const Object3DPtr &child);

    void remove_child(const Object3DPtr &child, bool recursive = false);

    void set_parent(const Object3DPtr &parent);

    vierkant::transform_t global_transform() const;

    void set_global_transform(const vierkant::transform_t &t);

    bool global_enable() const;

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
        return m_registry->template emplace_or_replace<T>(m_entity, component);
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
        if(m_registry->try_get<T>(m_entity))
        {
            m_registry->remove<T>(m_entity);
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
        return m_registry->try_get<T>(m_entity);
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
        return m_registry->try_get<T>(m_entity);
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

    //! set of tags
    std::set<std::string> tags;

    //! user definable name
    std::string name;

    //! enabled hint, can be used by Visitors
    bool enabled = true;

    //! local transformation of this object
    vierkant::transform_t transform = {};

    //! a list of child-objects
    std::vector<Object3DPtr> children;

    VIERKANT_ENABLE_AS_COMPONENT();

protected:
    explicit Object3D(entt::registry *registry, std::string name = "");

private:
    friend class crocore::fixed_size_free_list<vierkant::Object3D>;
    Object3D *m_parent = nullptr;

    entt::registry *m_registry = nullptr;

    entt::entity m_entity;
};

}// namespace vierkant
