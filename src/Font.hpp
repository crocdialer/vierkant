//  Font.h
//
//  Created by Fabian Schmidt on 3/9/13.

#pragma once

#include <crocore/Image.hpp>
#include "vierkant/Image.hpp"
#include "vierkant/intersection.hpp"
#include "vierkant/Mesh.hpp"

namespace vierkant {

class Font
{
public:

    enum class Align
    {
        LEFT, CENTER, RIGHT
    };

    Font();

    void load(vierkant::DevicePtr device, const std::string &the_path, size_t the_size, bool use_sdf = false);

    const std::string path() const;

    vierkant::ImagePtr glyph_texture() const;

    vierkant::ImagePtr sdf_texture() const;

    vierkant::AABB create_aabb(const std::string &theText) const;

    crocore::ImagePtr create_image(const std::string &theText, const glm::vec4 &theColor = glm::vec4(1)) const;

    vierkant::ImagePtr create_texture(vierkant::DevicePtr device, const std::string &theText, const glm::vec4 &theColor = glm::vec4(1)) const;

//    vierkant::MeshPtr create_mesh(const std::string &theText, const glm::vec4 &theColor = glm::vec4(1)) const;

    vierkant::Object3DPtr create_text_object(const std::string &the_text,
                                             Align the_align = Align::LEFT,
                                             uint32_t the_linewidth = 0,
                                             uint32_t the_lineheight = 0) const;

//    vierkant::Object3DPtr create_text_object(std::list<std::string> the_lines,
//                                             Align the_align = Align::LEFT,
//                                             uint32_t the_linewidth = 0,
//                                             uint32_t the_lineheight = 0) const;

    template<template<typename, typename> class Collection, typename T = std::string>
    vierkant::Object3DPtr create_text_object(const Collection<T, std::allocator<T>> &the_lines,
                                             Align the_align = Align::LEFT,
                                             uint32_t the_linewidth = 0,
                                             uint32_t the_lineheight = 0) const
    {
        return create_text_object(std::list<std::string>(std::begin(the_lines),
                                                         std::end(the_lines)),
                                  the_align, the_linewidth, the_lineheight);
    }

    uint32_t font_size() const;

    uint32_t line_height() const;

    void set_line_height(uint32_t the_line_height);

    bool use_sdf() const;

    void set_use_sdf(bool b);

private:
    std::shared_ptr<struct FontImpl> m_impl;

public:
    explicit operator bool() const { return m_impl.get(); }

    void reset() { m_impl.reset(); }
};

}// namespace
