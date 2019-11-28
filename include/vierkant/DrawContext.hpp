//
// Created by crocdialer on 3/2/19.
//

#pragma once

#include "vierkant/Renderer.hpp"
#include "vierkant/Font.hpp"

namespace vierkant
{

class DrawContext
{
public:

    DrawContext() = default;

    DrawContext(const DrawContext &) = delete;

    explicit DrawContext(vierkant::DevicePtr device);

    void draw_text(vierkant::Renderer &renderer, const std::string &text, const FontPtr &font, const glm::vec2 &pos,
                   const glm::vec4 &color = glm::vec4(1));

    void draw_image(vierkant::Renderer &renderer, const vierkant::ImagePtr &image,
                    const crocore::Area_<int> &area = {});

    void draw_boundingbox(vierkant::Renderer &renderer, const vierkant::AABB &aabb, const glm::mat4 &model_view,
                          const glm::mat4 &projection);

private:

    const shader_stage_map_t &shader_stages(vierkant::ShaderType type);

    vierkant::DevicePtr m_device;
    std::map<vierkant::ShaderType, shader_stage_map_t> m_shader_stage_cache;
    Renderer::drawable_t m_drawable_text = {};
    Renderer::drawable_t m_drawable_image = {};
    Renderer::drawable_t m_drawable_aabb = {};
};

}