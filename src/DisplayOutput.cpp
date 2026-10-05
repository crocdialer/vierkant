#include "vierkant/DisplayOutput.hpp"
#include <vierkant/shaders_slang.hpp>

namespace vierkant
{

DisplayOutput::DisplayOutput(const vierkant::DevicePtr &device)
{
    m_drawable.num_vertices = 3;
    m_drawable.use_own_buffers = true;
    m_drawable.share_material = false;
    m_drawable.pipeline_format.depth_test = false;
    m_drawable.pipeline_format.depth_write = false;
    m_drawable.pipeline_format.blend_state.blendEnable = false;
    m_drawable.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_drawable.pipeline_format.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    m_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::fullscreen::texture_slang);
    m_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::fullscreen::display_output_slang);

    vierkant::descriptor_t &desc_textures = m_drawable.descriptors[0];
    desc_textures.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_textures.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

    vierkant::descriptor_t &desc_params = m_drawable.descriptors[1];
    desc_params.type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
    desc_params.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

    vierkant::Image::Format fmt;
    fmt.extent = {1, 1, 1};
    fmt.format = VK_FORMAT_R8G8B8A8_UNORM;
    fmt.name = "display_output_empty_ui";
    uint32_t v = 0;
    m_empty_ui = vierkant::Image::create(device, &v, fmt);
}

void DisplayOutput::draw(vierkant::Rasterizer &renderer, const vierkant::ImagePtr &scene,
                         const vierkant::ImagePtr &ui, Encoding encoding) const
{
    if(!scene) { return; }

    struct params_t
    {
        float exposure;
        float gamma;
        float peak_nits;
        float paper_white_nits;
        uint32_t encoding;
    };

    auto drawable = m_drawable;
    drawable.descriptors[0].images = {scene, ui ? ui : m_empty_ui};
    drawable.descriptors[1].inline_uniform_block.resize(sizeof(params_t));
    auto params = reinterpret_cast<params_t *>(drawable.descriptors[1].inline_uniform_block.data());
    params->exposure = settings.exposure;
    params->gamma = settings.gamma;
    params->peak_nits = settings.peak_nits;
    params->paper_white_nits = settings.paper_white_nits;
    params->encoding = static_cast<uint32_t>(encoding);

    drawable.pipeline_format.scissor.extent.width = static_cast<uint32_t>(renderer.viewport.width);
    drawable.pipeline_format.scissor.extent.height = static_cast<uint32_t>(renderer.viewport.height);
    renderer.stage_drawable(std::move(drawable));
}

}// namespace vierkant
