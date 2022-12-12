//
// Created by crocdialer on 30/4/19.
//

#pragma once

#include <set>
#include <list>
#include <optional>

#include <entt/entity/registry.hpp>
#include <crocore/crocore.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/animation.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Object3D);

class Object3D : public std::enable_shared_from_this<Object3D>
{
public:

    //! signature for a function to retrieve a combined AABB
    using aabb_fn_t = std::function<vierkant::AABB(const std::optional<vierkant::animation_state_t> &)>;

    //! signature for a function to retrieve all sub-AABBs
    using sub_aabb_fn_t = std::function<std::vector<vierkant::AABB>(
            const std::optional<vierkant::animation_state_t> &)>;

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
    inline AABB aabb() const
    {
        auto aabb_fn_ptr = get_component_ptr<aabb_fn_t>();
        auto animation_state_ptr = get_component_ptr<vierkant::animation_state_t>();
        std::optional<vierkant::animation_state_t> dummy;
        if(aabb_fn_ptr){ return (*aabb_fn_ptr)(animation_state_ptr ? *animation_state_ptr : dummy); }
        auto aabb_ptr = get_component_ptr<vierkant::AABB>();
        return aabb_ptr ? *aabb_ptr : AABB();
    };

    OBB obb() const;

    inline std::vector<AABB> sub_aabbs() const
    {
        auto sub_aabb_fn_ptr = get_component_ptr<sub_aabb_fn_t>();
        auto animation_state_ptr = get_component_ptr<vierkant::animation_state_t>();
        std::optional<vierkant::animation_state_t> dummy;
        if(sub_aabb_fn_ptr){ return (*sub_aabb_fn_ptr)(animation_state_ptr ? *animation_state_ptr : dummy); }
        return {};
    }

    virtual void accept(class Visitor &theVisitor);

    /**
     * @brief   'add_component' can be used to attach an arbitrary component to this object's entity.
     *
     * @tparam  T           type of the component
     * @param   component   optional arbitrary component. will be copied if provided, otherwise default-constructed
     * @return  a reference for the newly created component.
     */
    template<typename T>
    inline T &add_component(const T &component = {})
    {
        if(auto reg = m_registry.lock())
        {
            return reg->template emplace<T>(m_entity, component);
        }
        throw std::runtime_error("error adding component: no registry defined");
    }

    /**
     * @brief   'has_component' can be used to determine if a given type exists among an object's components.
     *
     * @tparam  T   type of the component
     * @return  true, if a component of the provided type exists.
     */
    template<typename T>
    inline bool has_component() const{ return get_component_ptr<T>(); }

    /**
     * @brief   'get_component_ptr' can be used to retrieve a pointer to an object's component, if existing.
     *
     * @tparam  T   type of the component
     * @return  a pointer to a component of the provided type, if available. nullptr otherwise.
     */
    template<typename T>
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
    template<typename T>
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
    template<typename T>
    inline T &get_component()
    {
        auto ptr = get_component_ptr<T>();
        if(ptr){ return *ptr; }
        throw std::runtime_error("component does not exist");
    }

    /**
     * @brief   get_component can be used to retrieve a reference to an object's component, if existing.
     *          will throw if no object of the requested type could be found.
     *
     * @tparam  T   type of the component
     * @return  a reference to an associated component of the provided type.
     */
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
    explicit Object3D(const std::shared_ptr<entt::registry> &registry,
                      std::string name = "");

private:

    std::weak_ptr<Object3D> m_parent;

    std::weak_ptr<entt::registry> m_registry;

    entt::entity m_entity;
};

}//namespace
