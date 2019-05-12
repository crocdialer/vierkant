// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __
//
// Copyright (C) 2012-2016, Fabian Schmidt <crocdialer@googlemail.com>
//
// It is distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __

#include <algorithm>
#include "vierkant/Object3D.hpp"
#include "vierkant/Visitor.hpp"

namespace vierkant {
    
    uint32_t Object3D::s_id_pool = 0;
    
    // static factory
    Object3DPtr Object3D::create(const std::string &the_name)
    {
        auto ret = Object3DPtr(new Object3D());
        if(!the_name.empty()){ ret->set_name(the_name); }
        return ret;
    }
    
    Object3D::Object3D():
    m_id(s_id_pool++),
    m_name("Object3D_" + std::to_string(m_id)),
    m_enabled(true),
    m_billboard(false),
    m_transform(1)
    {

    }
    
    void Object3D::set_rotation(const glm::quat &theRot)
    {
        glm::vec3 pos_tmp(position()), scale_tmp(scale());
        m_transform = glm::mat4_cast(theRot);
        set_position(pos_tmp);
        set_scale(scale_tmp);
    }
    
    void Object3D::set_rotation(const glm::mat3 &theRot)
    {
        glm::vec3 pos_tmp(position()), scale_tmp(scale());
        m_transform = glm::mat4(theRot);
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
    
    void Object3D::set_look_at(const glm::vec3 &theLookAt, const glm::vec3 &theUp)
    {
        set_transform(glm::inverse(glm::lookAt(position(), theLookAt, theUp)) * glm::scale(glm::mat4(), scale()));
    }
    
    void Object3D::set_look_at(const Object3DPtr &theLookAt)
    {
        set_look_at(-theLookAt->position() + position(), theLookAt->up());
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
        while (ancestor)
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
        glm::mat4 parent_trans_inv = parent() ? glm::inverse(parent()->global_transform()) : glm::mat4();
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
    
    void Object3D::set_global_scale(const glm::vec3 &the_scale)
    {
        glm::vec3 parent_scale = parent() ? parent()->global_scale() : glm::vec3(1);
        set_scale(the_scale / parent_scale);
    }
    
    void Object3D::set_parent(const Object3DPtr &the_parent)
    {
        // detach object from former parent
        if(Object3DPtr p = parent())
        {
            p->remove_child(shared_from_this());
        }

        if(the_parent)
        {
            the_parent->add_child(shared_from_this());
        }
        else
        {
            m_parent.reset();
        }
    }
    
    void Object3D::add_child(const Object3DPtr &the_child)
    {
        if(the_child)
        {
            // avoid cyclic refs -> new child must not be an ancestor
            Object3DPtr ancestor = parent();
            while(ancestor)
            {
                if(ancestor == the_child) return;
                ancestor = ancestor->parent();
            }

            the_child->set_parent(Object3DPtr());
            the_child->m_parent = shared_from_this();

            // prevent multiple insertions
            if(std::find(m_children.begin(), m_children.end(), the_child) == m_children.end())
            {
                m_children.push_back(the_child);
            }
        }
    }
    
    void Object3D::remove_child(const Object3DPtr &the_child, bool recursive)
    {
        std::list<Object3DPtr>::iterator it = std::find(m_children.begin(), m_children.end(), the_child);
        if(it != m_children.end())
        {
            m_children.erase(it);
            if(the_child){the_child->set_parent(Object3DPtr());}
        }
        // not a direct descendant, go on recursive if requested
        else if(recursive)
        {
            for(auto &c : children())
            {
                c->remove_child(the_child, recursive);
            }
        }
    }
    
    AABB Object3D::aabb() const
    {
        AABB ret;
        glm::mat4 global_trans = global_transform();
        ret.transform(global_trans);

        for(auto &c :children())
        {
            if(c->enabled()){ ret += c->aabb(); }
        }
        return ret;
    }

    OBB Object3D::obb() const
    {
        OBB ret(aabb(), glm::mat4());
        return ret;
    }
    
    void Object3D::add_tag(const std::string& the_tag, bool recursive)
    {
        m_tags.insert(the_tag);
        
        if(recursive)
        {
            for(auto &c : children()){ c->add_tag(the_tag, recursive); }
        }

    }
    
    void Object3D::remove_tag(const std::string& the_tag, bool recursive)
    {
        m_tags.erase(the_tag);
        
        if(recursive)
        {
            for(auto &c : children()){ c->remove_tag(the_tag, recursive); }
        }
    }
    
    void Object3D::accept(Visitor &theVisitor)
    {
        theVisitor.visit(*this);
    }

}//namespace
