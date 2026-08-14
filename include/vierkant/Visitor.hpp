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

#include <stack>

#include "vierkant/Camera.hpp"
#include "vierkant/Object3D.hpp"

namespace vierkant
{

class Visitor
{
public:
    virtual ~Visitor() = default;
    Visitor() = default;

    virtual void visit(vierkant::Object3D &object)
    {
        if(should_visit(object))
        {
            for(Object3DPtr &child: object.children) { child->accept(*this); }
        }
    }
    virtual bool should_visit(vierkant::Object3D &) const { return true; }
};

template<typename T>
class SelectVisitor : public Visitor
{
public:
    explicit SelectVisitor(uint32_t layer_mask_ = LAYER_ALL, bool select_only_enabled_ = true)
        : layer_mask(layer_mask_), select_only_enabled(select_only_enabled_)
    {}
    void visit(T &object) override
    {
        if(should_visit(object))
        {
            objects.push_back(&object);
            Visitor::visit(static_cast<Object3D &>(object));
        }
    };

    bool should_visit(vierkant::Object3D &object) const override
    { return (object.enabled || !select_only_enabled) && (object.layers & layer_mask); }

    std::vector<T *> objects = {};

    uint32_t layer_mask = LAYER_ALL;

    bool select_only_enabled = true;
};

class LambdaVisitor : public Visitor
{
public:
    using visit_fn_t = std::function<bool(vierkant::Object3D &object)>;

    void traverse(vierkant::Object3D &object, visit_fn_t fn)
    {
        m_lambda = std::move(fn);
        object.accept(*this);
    }

    void visit(vierkant::Object3D &object) override
    {
        if(m_lambda && m_lambda(object))
        {
            for(Object3DPtr &child: object.children) { child->accept(*this); }
        }
    };

    visit_fn_t m_lambda;
};

}// namespace vierkant
