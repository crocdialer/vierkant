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
#include "vierkant/Mesh.hpp"
#include "vierkant/Camera.hpp"

namespace vierkant
{

class Visitor
{
public:

    explicit Visitor(bool visit_only_enabled = true) :
            m_visit_only_enabled(visit_only_enabled)
    {

    }

    inline bool visit_only_enabled() const{ return m_visit_only_enabled; }

    inline void set_visit_only_enabled(bool b){ m_visit_only_enabled = b; }

    virtual void visit(vierkant::Object3D &object)
    {
        if(should_visit(object))
        {
            for(Object3DPtr &child : object.children()){ child->accept(*this); }
        }
    }

    virtual void visit(vierkant::Mesh &mesh){ visit(static_cast<Object3D &>(mesh)); };

    virtual void visit(vierkant::Camera &camera){ visit(static_cast<Object3D &>(camera)); };

    virtual bool should_visit(vierkant::Object3D &object){ return true; }

private:

    bool m_visit_only_enabled;
};

template<typename T>
class SelectVisitor : public Visitor
{
public:

    explicit SelectVisitor(std::set<std::string> tags = {}, bool select_only_enabled = true) :
            Visitor(select_only_enabled),
            tags(std::move(tags)){}

    void visit(T &object) override
    {
        if(should_visit(object))
        {
            objects.push_back(&object);
            Visitor::visit(static_cast<Object3D &>(object));
        }
    };

    bool should_visit(vierkant::Object3D &object) override
    {
        return (object.enabled() || !visit_only_enabled()) && check_tags(tags, object.tags());
    }

    std::vector<T *> objects;

    std::set<std::string> tags;
};

}//namespace
