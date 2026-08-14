#include "vierkant/Object3D.hpp"
#include "vierkant/Visitor.hpp"

namespace vierkant
{

class ObjectStoreImpl final : public ObjectStore
{
public:
    ObjectStoreImpl(const uint32_t max_num_objects, const uint32_t page_size) : m_free_list(max_num_objects, page_size)
    {}
    [[nodiscard]] const std::shared_ptr<entt::registry> &registry() const override { return m_registry; }

    Object3DPtr create_object() override
    {
        uint32_t index = m_free_list.create(m_registry.get());
        return {&m_free_list.get(index), [this, index](Object3D *) { m_free_list.destroy(index); }};
    }

    Object3DPtr clone(const vierkant::Object3D *object) override
    {
        if(!object) return {};

        // stack for iterative traversal
        struct clone_item_t
        {
            const vierkant::Object3D *src;
            Object3DPtr dst;
        };
        std::stack<clone_item_t> stack;

        Object3DPtr root_clone = create_object();
        stack.push({object, root_clone});

        while(!stack.empty())
        {
            auto [src_obj, dst_obj] = std::move(stack.top());
            stack.pop();

            dst_obj->name = src_obj->name;
            dst_obj->remove_component<Object3D *>();
            dst_obj->enabled = src_obj->enabled;
            dst_obj->layers = src_obj->layers;

            // copy entt-components, this includes a potential transform_component_t
            for(auto [id, storage]: m_registry->storage())
            {
                if(storage.contains(static_cast<entt::entity>(src_obj->id())))
                {
                    storage.push(static_cast<entt::entity>(dst_obj->id()),
                                 storage.value(static_cast<entt::entity>(src_obj->id())));
                }
            }
            dst_obj->add_component(dst_obj.get());

            // the copied cache belongs to the source's position in the hierarchy
            dst_obj->invalidate_global_transform();

            // clone children iteratively
            for(const auto &child: src_obj->children)
            {
                Object3DPtr child_clone = create_object();
                dst_obj->add_child(child_clone);
                stack.push({child.get(), child_clone});
            }
        }
        return root_clone;
    }

    ~ObjectStoreImpl() override = default;

private:
    std::shared_ptr<entt::registry> m_registry = std::make_shared<entt::registry>();
    crocore::fixed_size_free_list<vierkant::Object3D> m_free_list;
};

std::unique_ptr<ObjectStore> create_object_store(uint32_t max_num_objects, uint32_t page_size)
{
    return std::make_unique<ObjectStoreImpl>(max_num_objects, page_size);
}

uint64_t last_inherited_flag_update(const vierkant::Object3D *object, flag_component_t::FlagEnum flag)
{
    uint64_t ret = 0;
    while(object)
    {
        if(auto *flag_cmp = object->get_component_ptr<flag_component_t>())
        {
            ret = std::max(ret, flag_cmp->timestamp(flag));
        }
        object = object->parent();
    }
    return ret;
}

bool has_inherited_flag(const vierkant::Object3D *object, uint32_t flag_bits)
{
    while(object)
    {
        if(auto *flag_cmp = object->get_component_ptr<flag_component_t>())
        {
            if((flag_cmp->flags & flag_bits) == flag_bits) { return true; }
        }
        object = object->parent();
    }
    return false;
}

Object3D::Object3D(entt::registry *registry, std::string name_) : name(std::move(name_)), m_registry(registry)
{
    if(registry)
    {
        m_entity = m_registry->create();
        add_component(this);
    }
    if(name.empty()) { name = "Object3D_" + std::to_string(id()); }
}

Object3D::~Object3D() noexcept
{
    for(const auto &child: children)
    {
        if(child)
        {
            // orphaned children lose an ancestor, their cached globals are stale now
            child->m_parent = nullptr;
            child->invalidate_global_transform();
        }
    }

    if(m_registry) { m_registry->destroy(m_entity); }
}

const vierkant::transform_t *Object3D::transform() const
{
    auto *transform_cmp = get_component_ptr<transform_component_t>();
    return transform_cmp ? &transform_cmp->transform : nullptr;
}

void Object3D::set_transform(const vierkant::transform_t &t)
{
    if(auto *transform_cmp = get_component_ptr<transform_component_t>()) { transform_cmp->transform = t; }
    else { add_component<transform_component_t>({.transform = t}); }
    invalidate_global_transform();

    // render-hint: 'changed this frame', cleared by Scene::update
    if(auto *flag_cmp_ptr = get_component_ptr<flag_component_t>())
    {
        flag_cmp_ptr->flags |= flag_component_t::DIRTY_TRANSFORM;
    }
    else
    {
        auto &flag_cmp = add_component<flag_component_t>();
        flag_cmp.flags |= flag_component_t::DIRTY_TRANSFORM;
    }
}

void Object3D::remove_transform()
{
    remove_component<transform_component_t>();
    invalidate_global_transform();
}

uint8_t Object3D::transform_space() const
{
    auto *transform_cmp = get_component_ptr<transform_component_t>();
    return transform_cmp ? transform_cmp->space : static_cast<uint8_t>(transform_component_t::RELATIVE);
}

void Object3D::set_transform_space(uint8_t space)
{
    if(auto *transform_cmp = get_component_ptr<transform_component_t>()) { transform_cmp->space = space; }
    else { add_component<transform_component_t>({.space = space}); }
    invalidate_global_transform();
}

void Object3D::invalidate_global_transform()
{
    // a dirty object may still have clean descendants, so the whole sub-tree has to be visited
    std::stack<Object3D *> stack;
    stack.push(this);

    while(!stack.empty())
    {
        auto *object = stack.top();
        stack.pop();

        if(auto *transform_cmp = object->get_component_ptr<transform_component_t>()) { transform_cmp->dirty = true; }

        for(const auto &child: object->children)
        {
            // a fully absolute child ignores its ancestors, so neither it nor its sub-tree is affected.
            // partially absolute children still depend on the parent via their relative channels.
            if(child->transform_space() != transform_component_t::ABSOLUTE) { stack.push(child.get()); }
        }
    }
}

vierkant::transform_t Object3D::global_transform() const
{
    auto *transform_cmp = get_component_ptr<transform_component_t>();

    // no transform of our own, we are wherever our parent is
    if(!transform_cmp) { return parent() ? parent()->global_transform() : vierkant::transform_t{}; }

    if(transform_cmp->dirty)
    {
        transform_cmp->global = combine_with_parent(transform_cmp->transform, transform_cmp->space);
        transform_cmp->dirty = false;
    }
    return transform_cmp->global;
}

vierkant::transform_t Object3D::combine_with_parent(const vierkant::transform_t &t, uint8_t space) const
{
    if(!parent() || space == transform_component_t::ABSOLUTE) { return t; }

    // compose first, then let the absolute channels override. deliberately not a per-channel
    // composition: operator* falls back to a mat4-roundtrip for non-uniform scaling, which entangles
    // the channels, so there is no separable per-channel formula that stays correct there.
    auto ret = parent()->global_transform() * t;
    if(space & transform_component_t::ABSOLUTE_TRANSLATION) { ret.translation = t.translation; }
    if(space & transform_component_t::ABSOLUTE_ROTATION) { ret.rotation = t.rotation; }
    if(space & transform_component_t::ABSOLUTE_SCALE) { ret.scale = t.scale; }
    return ret;
}

vierkant::transform_t Object3D::relative_transform() const
{
    auto *transform_cmp = get_component_ptr<transform_component_t>();
    if(!transform_cmp) { return {}; }

    // nothing to undo for a purely relative object, which is the common case
    if(!parent() || transform_cmp->space == transform_component_t::RELATIVE) { return transform_cmp->transform; }

    auto parent_global = parent()->global_transform();
    auto global = global_transform();
    return vierkant::is_identity(parent_global) ? global : vierkant::inverse(parent_global) * global;
}

/**
 * NOTE: this roundtrips exactly (global_transform() reproduces 't') for any space, as long as the
 * parent-chain scales uniformly. on a *sheared* chain an absolute rotation-channel breaks it: the
 * relative channels are a QR pre-image whose projected scale is derived from the rotation, so
 * overwriting the rotation invalidates the scale. there is no closed-form fix - 'P * L' is a lossy
 * projection. covered by TestObject3D.set_global_transform_roundtrip_sheared_parent, and in-contract
 * anyway: transform_t cannot represent shear, see transform.hpp.
 */
void Object3D::set_global_transform(const vierkant::transform_t &t)
{
    const uint8_t space = transform_space();

    if(!parent() || space == transform_component_t::ABSOLUTE) { set_transform(t); }
    else
    {
        // the parent's global is cached, and transform_t::inverse has a uniform-scale fast-path,
        // so this avoids a mat4-inverse + glm::decompose unless the parent-chain scales non-uniformly
        auto rel = vierkant::inverse(parent()->global_transform()) * t;

        // absolute channels are stored in world-space, they need no conversion
        if(space & transform_component_t::ABSOLUTE_TRANSLATION) { rel.translation = t.translation; }
        if(space & transform_component_t::ABSOLUTE_ROTATION) { rel.rotation = t.rotation; }
        if(space & transform_component_t::ABSOLUTE_SCALE) { rel.scale = t.scale; }
        set_transform(rel);
    }
}

bool Object3D::global_enable() const
{
    const Object3D *object = this;
    while(object)
    {
        if(!object->enabled) { return false; }
        object = object->parent();
    }
    return true;
}

void Object3D::set_parent(const Object3DPtr &parent_object)
{
    // detach object from former parent
    if(auto p = parent()) { p->remove_child(shared_from_this()); }

    if(parent_object)
    {
        parent_object->add_child(shared_from_this());
        m_registry = parent_object->m_registry;
    }
    else
    {
        m_parent = nullptr;
        invalidate_global_transform();
    }
}

void Object3D::add_child(const Object3DPtr &child)
{
    if(child)
    {
        // avoid cyclic refs -> new child must not be an ancestor
        const Object3D *ancestor = parent();

        while(ancestor)
        {
            if(ancestor == child.get()) { return; }
            ancestor = ancestor->parent();
        }

        child->set_parent(Object3DPtr());
        child->m_parent = this;

        // the child gained an ancestor-chain
        child->invalidate_global_transform();

        // prevent multiple insertions
        if(std::ranges::find(children, child) == children.end()) { children.push_back(child); }
    }
}

void Object3D::remove_child(const Object3DPtr &child, bool recursive)
{
    if(const auto it = std::ranges::find(children, child); it != children.end())
    {
        children.erase(it);
        if(child)
        {
            child->set_parent(nullptr);

            // the child lost its ancestor-chain
            child->invalidate_global_transform();
        }
    }
    else if(recursive)
    {
        // not a direct descendant, go on recursive if requested
        for(const auto &c: children) { c->remove_child(child, recursive); }
    }
}

AABB Object3D::aabb() const
{
    AABB ret;
    if(auto *aabb_component_ptr = get_component_ptr<aabb_component_t>();
       aabb_component_ptr && aabb_component_ptr->aabb_fn)
    {
        ret += aabb_component_ptr->aabb_fn(*this);
    }
    for(const auto &child: children)
    {
        auto child_aabb = child->aabb();

        // a child's stored transform is not parent-relative if any of its channels is absolute
        if(child->has_component<transform_component_t>())
        {
            child_aabb = child_aabb.transform(child->relative_transform());
        }
        ret += child_aabb;
    }
    return ret;
}

std::vector<AABB> Object3D::sub_aabbs() const
{
    if(auto *aabb_component_ptr = get_component_ptr<aabb_component_t>();
       aabb_component_ptr && aabb_component_ptr->sub_aabb_fn)
    {
        return aabb_component_ptr->sub_aabb_fn(*this);
    }
    return {};
}

OBB Object3D::obb() const { return {aabb(), glm::mat4(1)}; }

void Object3D::accept(Visitor &theVisitor) { theVisitor.visit(*this); }

}// namespace vierkant
