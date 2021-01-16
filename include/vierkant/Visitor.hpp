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
#include "vierkant/MeshNode.hpp"
#include "vierkant/Camera.hpp"

namespace vierkant
{

class Visitor
{
public:

    Visitor() = default;

    virtual void visit(vierkant::Object3D &object)
    {
        if(should_visit(object))
        {
            for(Object3DPtr &child : object.children()){ child->accept(*this); }
        }
    }

    virtual void visit(vierkant::MeshNode &mesh){ visit(static_cast<Object3D &>(mesh)); };

    virtual void visit(vierkant::Camera &camera){ visit(static_cast<Object3D &>(camera)); };

    virtual bool should_visit(vierkant::Object3D &object) = 0;

};

template<typename T>
class SelectVisitor : public Visitor
{
public:

    explicit SelectVisitor(std::set<std::string> tags = {}, bool select_only_enabled = true):
            tags(std::move(tags)),
            select_only_enabled(select_only_enabled){}

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
        return (object.enabled() || !select_only_enabled) && check_tags(tags, object.tags());
    }

    std::vector<T *> objects = {};

    std::set<std::string> tags = {};

    bool select_only_enabled = true;
};

}//namespace
