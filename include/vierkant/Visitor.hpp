// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __
//
// Copyright (C) 2012-2016, Fabian Schmidt <crocdialer@googlemail.com>
//
// It is distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __

//  Visitor.hpp
//
//  Created by Croc Dialer on 31/03/15.

#pragma once

#include <set>
#include <stack>

#include "vierkant/Object3D.hpp"
//#include "Mesh.hpp"
//#include "Light.hpp"
//#include "Camera.hpp"

namespace vierkant {

class Visitor
{
public:

    explicit Visitor(bool visit_only_enabled = true) :
            m_visit_only_enabled(visit_only_enabled)
    {
        m_transform_stack.push(glm::mat4());
    }

    inline const std::stack<glm::mat4> &transform_stack() const { return m_transform_stack; };

    inline std::stack<glm::mat4> &transform_stack() { return m_transform_stack; };

    inline bool visit_only_enabled() const { return m_visit_only_enabled; }

    inline void set_visit_only_enabled(bool b) { m_visit_only_enabled = b; }

    virtual void visit(Object3D &theNode)
    {
        if(theNode.enabled() || !visit_only_enabled())
        {
            m_transform_stack.push(m_transform_stack.top() * theNode.transform());
            for(Object3DPtr &child : theNode.children()){ child->accept(*this); }
            m_transform_stack.pop();
        }
    }
//        virtual void visit(gl::Mesh &theNode){ visit(static_cast<Object3D&>(theNode)); };
//        virtual void visit(gl::Light &theNode){ visit(static_cast<Object3D&>(theNode)); };
//        virtual void visit(gl::Camera &theNode){ visit(static_cast<Object3D&>(theNode)); };

protected:

    inline bool check_tags(const std::set<std::string> &filter_tags,
                           const std::set<std::string> &obj_tags)
    {
        for(const auto &t : obj_tags)
        {
            if(crocore::contains(filter_tags, t)){ return true; }
        }
        return filter_tags.empty();
    }

private:
    std::stack<glm::mat4> m_transform_stack;
    bool m_visit_only_enabled;
};

template<typename T>
class SelectVisitor : public Visitor
{
public:

    explicit SelectVisitor(std::set<std::string> tags = {}, bool select_only_enabled = true) :
            Visitor(select_only_enabled),
            m_tags(std::move(tags)) {}

    void visit(T &theNode) override
    {
        if(theNode.enabled() || !visit_only_enabled())
        {
            if(check_tags(m_tags, theNode.tags())){ m_objects.push_back(&theNode); }
            Visitor::visit(static_cast<Object3D &>(theNode));
        }
    };

    const std::set<std::string> &tags() const { return m_tags; }

    void set_tags(const std::set<std::string> &the_tags) { m_tags = the_tags; }

    void add_tag(const std::string &the_tag) { m_tags.insert(the_tag); }

    void remove_tag(const std::string &the_tag) { m_tags.erase(the_tag); }

    void clear() { m_objects.clear(); }

    const std::list<T *> &get_objects() const { return m_objects; };

private:
    std::list<T *> m_objects;
    std::set<std::string> m_tags;
};

}//namespace
