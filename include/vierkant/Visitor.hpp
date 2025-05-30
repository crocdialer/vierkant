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

#include "vierkant/Camera.hpp"
#include "vierkant/Object3D.hpp"

namespace vierkant
{

/**
 * @brief   Utility to check if one set of tags contains at least one item from another set.
 *
 * @param   whitelist the tags that shall pass the check.
 * @param   obj_tags    the tags to check against the whitelist
 * @return
 */
inline static bool check_tags(const std::set<std::string> &whitelist, const std::set<std::string> &obj_tags)
{
    for(const auto &t: obj_tags)
    {
        if(whitelist.count(t)) { return true; }
    }
    return whitelist.empty();
}

class Visitor
{
public:
    Visitor() = default;

    virtual void visit(vierkant::Object3D &object)
    {
        if(should_visit(object))
        {
            for(Object3DPtr &child: object.children) { child->accept(*this); }
        }
    }
    virtual void visit(vierkant::Camera &camera) { visit(static_cast<Object3D &>(camera)); };
    virtual void visit(vierkant::PerspectiveCamera &camera) { visit(static_cast<Camera &>(camera)); };
    virtual void visit(vierkant::OrthoCamera &camera) { visit(static_cast<Camera &>(camera)); };
    virtual bool should_visit(vierkant::Object3D &) const { return true; };
};

template<typename T>
class SelectVisitor : public Visitor
{
public:
    explicit SelectVisitor(std::set<std::string> tags_ = {}, bool select_only_enabled_ = true)
        : tags(std::move(tags_)), select_only_enabled(select_only_enabled_)
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
    {
        return (object.enabled || !select_only_enabled) && check_tags(tags, object.tags);
    }

    std::vector<T *> objects = {};

    std::set<std::string> tags = {};

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
