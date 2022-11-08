//  Font.h
//
//  Created by Fabian Schmidt on 3/9/13.

#pragma once

#include <crocore/Image.hpp>
#include "vierkant/Image.hpp"
#include "vierkant/Mesh.hpp"
#include "vierkant/Object3D.hpp"

namespace vierkant {

DEFINE_CLASS_PTR(Font)

class Font
{
public:

    enum class Align
    {
        LEFT, CENTER, RIGHT
    };

    static FontPtr create(const vierkant::DevicePtr& device,
                          const std::string &path,
                          size_t size,
                          bool use_sdf = false);

    static FontPtr create(const vierkant::DevicePtr& device,
                          const std::vector<uint8_t> &data,
                          size_t size,
                          bool use_sdf = false);

    Font(const Font &) = delete;

    Font(Font &&) = delete;

    Font &operator=(Font other) = delete;

    vierkant::ImagePtr glyph_texture() const;

    vierkant::ImagePtr sdf_texture() const;

    vierkant::AABB create_aabb(const std::string &theText) const;

    crocore::ImagePtr create_image(const std::string &theText, const glm::vec4 &theColor = glm::vec4(1)) const;

    vierkant::ImagePtr create_texture(vierkant::DevicePtr device, const std::string &theText,
                                      const glm::vec4 &theColor = glm::vec4(1)) const;

    vierkant::MeshPtr create_mesh(const std::string &theText,
                                  const glm::vec4 &theColor = glm::vec4(1),
                                  float extrude = 0.f) const;

    uint32_t font_size() const;

    uint32_t line_height() const;

    void set_line_height(uint32_t the_line_height);

    bool use_sdf() const;

    void set_use_sdf(bool b);

private:

    Font(const vierkant::DevicePtr& device, const std::vector<uint8_t> &data, size_t size, bool use_sdf);

    std::unique_ptr<struct FontImpl> m_impl;
};

}// namespace
