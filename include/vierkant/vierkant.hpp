//
// Created by crocdialer on 3/2/19.
//

#pragma once

#include "vierkant/Instance.hpp"
#include "vierkant/Device.hpp"
#include "vierkant/Buffer.hpp"
#include "vierkant/Image.hpp"
#include "vierkant/Framebuffer.hpp"
#include "vierkant/Pipeline.hpp"
#include "vierkant/Mesh.hpp"
#include "vierkant/SwapChain.hpp"
#include "vierkant/Window.hpp"
#include "vierkant/Geometry.hpp"
#include "vierkant/Font.hpp"
#include "vierkant/Renderer.hpp"
#include "vierkant/intersection.hpp"

namespace vierkant {

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

private:

    const shader_stage_map_t& shader_stages(vierkant::ShaderType type);

    vierkant::DevicePtr m_device;
    std::map<vierkant::ShaderType, shader_stage_map_t> m_shader_stage_cache;
    Renderer::drawable_t m_drawable_text = {};
    Renderer::drawable_t m_drawable_image = {};
};

//void draw_mesh(vierkant::Renderer &renderer, const MeshPtr &mesh);

}
namespace vk = vierkant;