#include "vierkant/Object3D.hpp"

#include <vierkant/Scene.hpp>
#include "vierkant/Visitor.hpp"

namespace vierkant
{

uint32_t Object3D::s_id_pool = 0;

// static factory
Object3DPtr Object3D::create(const vierkant::SceneConstPtr &scene,
                             std::string name)
{
    return Object3DPtr(new Object3D(scene, std::move(name)));
}

Object3D::Object3D(const vierkant::SceneConstPtr &scene,
                   std::string name) :
        m_id(s_id_pool++),
        m_name(std::move(name)),
        m_enabled(true),
        m_billboard(false),
        m_transform(1),
        m_registry(scene ? scene->registry() : nullptr)
{
    if(m_name.empty()){ m_name = "Object3D_" + std::to_string(m_id); }
    if(m_registry){ m_entity = m_registry->create(); }
}

void Object3D::set_position(const glm::vec3 &pos)
{
    glm::vec3 &dst = *reinterpret_cast<glm::vec3 *>(&m_transform[3].x);
    dst = pos;
}

void Object3D::set_rotation(const glm::quat &rot)
{
    glm::vec3 pos_tmp(position()), scale_tmp(scale());
    m_transform = glm::mat4_cast(rot);
    set_position(pos_tmp);
    set_scale(scale_tmp);
}

void Object3D::set_rotation(const glm::mat3 &rot)
{
    glm::vec3 pos_tmp(position()), scale_tmp(scale());
    m_transform = glm::mat4(rot);
    set_position(pos_tmp);
    set_scale(scale_tmp);
}

void Object3D::set_rotation(float pitch, float yaw, float roll)
{
    glm::vec3 pos_tmp(position()), scale_tmp(scale());
    m_transform = glm::mat4_cast(glm::quat(glm::vec3(pitch, yaw, roll)));
    set_position(pos_tmp);
    set_scale(scale_tmp);
}

glm::quat Object3D::rotation() const
{
    return glm::normalize(glm::quat_cast(m_transform));
}

void Object3D::set_look_at(const glm::vec3 &lookAt, const glm::vec3 &up)
{
    set_transform(glm::inverse(glm::lookAt(position(), lookAt, up)) * glm::scale(glm::mat4(1), scale()));
}

void Object3D::set_look_at(const Object3DPtr &lookAt)
{
    set_look_at(-lookAt->position() + position(), lookAt->up());
}

void Object3D::set_scale(const glm::vec3 &s)
{
    glm::vec3 scale_vec = s / scale();
    m_transform = glm::scale(m_transform, scale_vec);
}

glm::mat4 Object3D::global_transform() const
{
    glm::mat4 ret = transform();
    Object3DPtr ancestor = parent();
    while(ancestor)
    {
        ret = ancestor->transform() * ret;
        ancestor = ancestor->parent();
    }
    return ret;
}

glm::vec3 Object3D::global_position() const
{
    glm::mat4 global_trans = global_transform();
    return global_trans[3].xyz();
}

glm::quat Object3D::global_rotation() const
{
    glm::mat4 global_trans = global_transform();
    return glm::normalize(glm::quat_cast(global_trans));
}

glm::vec3 Object3D::global_scale() const
{
    glm::mat4 global_trans = global_transform();
    return glm::vec3(glm::length(global_trans[0]),
                     glm::length(global_trans[1]),
                     glm::length(global_trans[2]));
}

void Object3D::set_global_transform(const glm::mat4 &transform)
{
    glm::mat4 parent_trans_inv = parent() ? glm::inverse(parent()->global_transform()) : glm::mat4(1);
    m_transform = parent_trans_inv * transform;
}

void Object3D::set_global_position(const glm::vec3 &position)
{
    glm::vec3 parent_pos = parent() ? parent()->global_position() : glm::vec3();
    set_position(position - parent_pos);
}

void Object3D::set_global_rotation(const glm::quat &rotation)
{
    glm::quat parent_rotation = parent() ? parent()->global_rotation() : glm::quat();
    set_rotation(glm::inverse(parent_rotation) * rotation);
}

void Object3D::set_global_scale(const glm::vec3 &scale)
{
    glm::vec3 parent_scale = parent() ? parent()->global_scale() : glm::vec3(1);
    set_scale(scale / parent_scale);
}

void Object3D::set_parent(const Object3DPtr &parent_object)
{
    // detach object from former parent
    if(auto p = parent()){ p->remove_child(shared_from_this()); }

    if(parent_object){ parent_object->add_child(shared_from_this()); }
    else{ m_parent.reset(); }
}

void Object3D::add_child(const Object3DPtr &child)
{
    if(child)
    {
        // avoid cyclic refs -> new child must not be an ancestor
        Object3DPtr ancestor = parent();

        while(ancestor)
        {
            if(ancestor == child){ return; }
            ancestor = ancestor->parent();
        }

        child->set_parent(Object3DPtr());
        child->m_parent = shared_from_this();

        // prevent multiple insertions
        if(std::find(m_children.begin(), m_children.end(), child) == m_children.end())
        {
            m_children.push_back(child);
        }
    }
}

void Object3D::remove_child(const Object3DPtr &child, bool recursive)
{
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if(it != m_children.end())
    {
        m_children.erase(it);
        if(child){ child->set_parent(nullptr); }
    }
    else if(recursive)
    {
        // not a direct descendant, go on recursive if requested
        for(auto &c: children())
        {
            c->remove_child(child, recursive);
        }
    }
}

AABB Object3D::aabb() const
{
    AABB ret;
    for(auto &c: children()){ if(c->enabled()){ ret += c->aabb().transform(c->transform()); }}
    return ret;
}

OBB Object3D::obb() const
{
    OBB ret(aabb(), glm::mat4(1));
    return ret;
}

void Object3D::add_tag(const std::string &tag, bool recursive)
{
    m_tags.insert(tag);

    if(recursive)
    {
        for(auto &c: children()){ c->add_tag(tag, recursive); }
    }

}

void Object3D::remove_tag(const std::string &tag, bool recursive)
{
    m_tags.erase(tag);

    if(recursive)
    {
        for(auto &c: children()){ c->remove_tag(tag, recursive); }
    }
}

void Object3D::accept(Visitor &theVisitor)
{
    theVisitor.visit(*this);
}

}//namespace
