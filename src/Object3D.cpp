#include "vierkant/Object3D.hpp"
#include "vierkant/Visitor.hpp"

namespace vierkant
{

class ObjectStoreImpl : public ObjectStore
{
public:
    ObjectStoreImpl(uint32_t max_num_objects, uint32_t page_size) : m_free_list(max_num_objects, page_size) {};
    [[nodiscard]] const std::shared_ptr<entt::registry> &registry() const override { return m_registry; }

    Object3DPtr create_object() override
    {
        uint32_t index = m_free_list.create(m_registry);
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
            dst_obj->tags = src_obj->tags;
            dst_obj->transform = src_obj->transform;

            // copy entt-components
            for(auto [id, storage]: m_registry->storage())
            {
                if(storage.contains(static_cast<entt::entity>(src_obj->id())))
                {
                    storage.push(static_cast<entt::entity>(dst_obj->id()),
                                 storage.value(static_cast<entt::entity>(src_obj->id())));
                }
            }
            dst_obj->add_component(dst_obj.get());

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

    virtual ~ObjectStoreImpl() = default;

private:
    std::shared_ptr<entt::registry> m_registry = std::make_shared<entt::registry>();
    crocore::fixed_size_free_list<vierkant::Object3D> m_free_list;
};

std::unique_ptr<ObjectStore> create_object_store(uint32_t max_num_objects, uint32_t page_size)
{
    return std::make_unique<ObjectStoreImpl>(max_num_objects, page_size);
}

glm::mat4 get_global_mat4(const vierkant::Object3D *obj)
{
    glm::mat4 ret = mat4_cast(obj->transform);
    Object3DPtr ancestor = obj->parent();
    while(ancestor)
    {
        ret = mat4_cast(ancestor->transform) * ret;
        ancestor = ancestor->parent();
    }
    return ret;
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
    return parent() ? transform_cast(get_global_mat4(this)) : transform;
}

void Object3D::set_global_transform(const vierkant::transform_t &t)
{
    transform = parent() ? transform_cast(glm::inverse(get_global_mat4(parent().get())) * mat4_cast(t)) : t;
}

bool Object3D::global_enable() const
{
    if(!enabled) { return false; }
    Object3DPtr ancestor = parent();
    while(ancestor)
    {
        if(!ancestor->enabled) { return false; }
        ancestor = ancestor->parent();
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
    if(aabb_component_ptr && aabb_component_ptr->aabb_fn) { ret += aabb_component_ptr->aabb_fn(*this); }
    for(const auto &child: children) { ret += child->aabb().transform(child->transform); }
    return ret;
}

std::vector<AABB> Object3D::sub_aabbs() const
{
    auto aabb_component_ptr = get_component_ptr<aabb_component_t>();
    if(aabb_component_ptr && aabb_component_ptr->sub_aabb_fn) { return aabb_component_ptr->sub_aabb_fn(*this); }
    return {};
}

OBB Object3D::obb() const
{
    OBB ret(aabb(), glm::mat4(1));
    return ret;
}

void Object3D::accept(Visitor &theVisitor) { theVisitor.visit(*this); }

}// namespace vierkant
