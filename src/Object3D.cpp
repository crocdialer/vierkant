#include "vierkant/Object3D.hpp"
#include "vierkant/Visitor.hpp"

namespace vierkant
{

// static factory
Object3DPtr Object3D::create(const std::shared_ptr<entt::registry> &registry, std::string name)
{
    return Object3DPtr(new Object3D(registry, std::move(name)));
}

Object3D::Object3D(const std::shared_ptr<entt::registry> &registry, std::string name_)
    : name(std::move(name_)), m_registry(registry)
{
    if(auto reg = m_registry.lock())
    {
        m_entity = reg->create();
        add_component(this);
    }
    if(name.empty()) { name = "Object3D_" + std::to_string(id()); }
}

Object3D::~Object3D() noexcept
{
    if(auto reg = m_registry.lock()) { reg->destroy(m_entity); }
}

vierkant::transform_t Object3D::global_transform() const
{
    auto ret = transform;
    Object3DPtr ancestor = parent();
    while(ancestor)
    {
        ret = ancestor->transform * ret;
        ancestor = ancestor->parent();
    }
    return ret;
}

void Object3D::set_global_transform(const vierkant::transform_t &t)
{
    auto parent_inverse = parent() ? vierkant::inverse(parent()->global_transform()) : vierkant::transform_t();
    transform = parent_inverse * t;
}

void Object3D::set_parent(const Object3DPtr &parent_object)
{
    // detach object from former parent
    if(auto p = parent())
    {
        p->remove_child(shared_from_this());
        if(auto reg = m_registry.lock())
        {
            reg->destroy(m_entity);
            m_registry = {};
            m_entity = {};
        }
    }

    if(parent_object)
    {
        parent_object->add_child(shared_from_this());
        m_registry = parent_object->m_registry;
    }
    else { m_parent.reset(); }
}

void Object3D::add_child(const Object3DPtr &child)
{
    if(child)
    {
        // avoid cyclic refs -> new child must not be an ancestor
        Object3DPtr ancestor = parent();

        while(ancestor)
        {
            if(ancestor == child) { return; }
            ancestor = ancestor->parent();
        }

        child->set_parent(Object3DPtr());
        child->m_parent = shared_from_this();

        // prevent multiple insertions
        if(std::find(children.begin(), children.end(), child) == children.end()) { children.push_back(child); }
    }
}

void Object3D::remove_child(const Object3DPtr &child, bool recursive)
{
    auto it = std::find(children.begin(), children.end(), child);
    if(it != children.end())
    {
        children.erase(it);
        if(child) { child->set_parent(nullptr); }
    }
    else if(recursive)
    {
        // not a direct descendant, go on recursive if requested
        for(auto &c: children) { c->remove_child(child, recursive); }
    }
}

AABB Object3D::aabb() const
{
    AABB ret;
    auto aabb_component_ptr = get_component_ptr<aabb_component_t>();
    auto animation_state_ptr = get_component_ptr<vierkant::animation_component_t>();
    if(aabb_component_ptr && aabb_component_ptr->aabb_fn)
    {
        ret += aabb_component_ptr->aabb_fn(animation_state_ptr ? *animation_state_ptr
                                                               : std::optional<vierkant::animation_component_t>());
    }
    else if(aabb_component_ptr) { ret += aabb_component_ptr->aabb; }
    for(const auto &child: children) { ret += child->aabb().transform(child->transform); }
    return ret;
}

std::vector<AABB> Object3D::sub_aabbs() const
{
    auto aabb_component_ptr = get_component_ptr<aabb_component_t>();
    auto animation_state_ptr = get_component_ptr<vierkant::animation_component_t>();
    if(aabb_component_ptr && aabb_component_ptr->sub_aabb_fn)
    {
        return aabb_component_ptr->sub_aabb_fn(animation_state_ptr ? *animation_state_ptr
                                                                   : std::optional<vierkant::animation_component_t>());
    }
    return {};
}

OBB Object3D::obb() const
{
    OBB ret(aabb(), glm::mat4(1));
    return ret;
}

void Object3D::accept(Visitor &theVisitor) { theVisitor.visit(*this); }

Object3DPtr Object3D::clone() const
{
    auto registry = m_registry.lock();
    auto ret = Object3D::create(registry);
    ret->remove_component<Object3D*>();

    ret->name = name;
    ret->enabled = enabled;
    ret->tags = tags;
    ret->transform = transform;

    if(registry)
    {
        for(auto [id, storage]: registry->storage())
        {
            if(storage.contains(m_entity)) { storage.push(ret->m_entity, storage.value(m_entity)); }
        }
    }
    ret->add_component(ret.get());
    for(const auto &child: children) { ret->add_child(child->clone()); }
    return ret;
}

}// namespace vierkant
