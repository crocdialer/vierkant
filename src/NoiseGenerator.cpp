#include "vierkant/NoiseGenerator.hpp"
#include "vierkant/shaders_slang.hpp"

namespace vierkant
{

std::unique_ptr<NoiseGenerator> NoiseGenerator::create(const DevicePtr &device, const create_info_t &create_info)
{ return std::unique_ptr<NoiseGenerator>(new NoiseGenerator(device, create_info)); }

NoiseGenerator::NoiseGenerator(const DevicePtr &device, const create_info_t &create_info)
{
    m_device = device;
    m_color_format = create_info.color_format;

    vierkant::Rasterizer::create_info_t renderer_info = {};
    renderer_info.num_frames_in_flight = 1;
    renderer_info.viewport.width = static_cast<float>(create_info.size.width);
    renderer_info.viewport.height = static_cast<float>(create_info.size.height);
    renderer_info.viewport.maxDepth = 1;
    renderer_info.pipeline_cache = create_info.pipeline_cache;
    renderer_info.descriptor_pool = create_info.descriptor_pool;
    m_renderer = vierkant::Rasterizer(device, renderer_info);

    vierkant::Framebuffer::create_info_t fb_info = {};
    fb_info.size = create_info.size;
    fb_info.color_attachment_format.format = create_info.color_format;
    fb_info.color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    fb_info.begin_rendering_info = {.clear_color_attachment = true, .clear_depth_attachment = false};
    m_framebuffer = vierkant::Framebuffer(device, fb_info);

    // clone fb-attachment, swizzle to (r, r, r, 1)
    m_out_image = m_framebuffer.color_attachment()->clone(
            {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE});

    m_params_buffer = vierkant::Buffer::create(device, nullptr, sizeof(params_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                               VMA_MEMORY_USAGE_CPU_TO_GPU);

    vierkant::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::fullscreen::texture_slang);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(vierkant::slang_shaders::fullscreen::noise_slang);
    drawable.pipeline_format.depth_test = false;
    drawable.pipeline_format.depth_write = false;
    drawable.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    drawable.num_vertices = 3;
    drawable.use_own_buffers = true;

    vierkant::descriptor_t desc_params = {};
    desc_params.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_params.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_params.buffers = {m_params_buffer};
    drawable.descriptors[0] = std::move(desc_params);

    m_drawable = std::move(drawable);
}

vierkant::ImagePtr NoiseGenerator::generate(VkCommandBuffer commandbuffer, glm::vec2 scale, float seed)
{
    auto *p = static_cast<params_t *>(m_params_buffer->map());
    p->seed = glm::vec4(seed);
    p->scale = scale;
    m_params_buffer->unmap();

    vierkant::begin_label(commandbuffer, {"NoiseGenerator::generate"});

    vierkant::Rasterizer::rendering_info_t rendering_info = {};
    rendering_info.command_buffer = commandbuffer;
    rendering_info.color_attachment_formats = m_framebuffer.color_attachment_formats();

    m_renderer.stage_drawable(m_drawable);
    m_framebuffer.begin_rendering(commandbuffer, {});
    m_renderer.render(rendering_info);
    m_framebuffer.end_rendering({.final_layout_color = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
    vierkant::end_label(commandbuffer);
    return m_out_image;
}

}// namespace vierkant
