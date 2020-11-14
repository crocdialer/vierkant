//
// Created by crocdialer on 10/28/20.
//

#include <vierkant/shaders.hpp>
#include <vierkant/Bloom.hpp>

namespace vierkant
{

BloomUPtr Bloom::create(const DevicePtr &device, const Bloom::create_info_t &create_info)
{
    return vierkant::BloomUPtr(new Bloom(device, create_info));
}

Bloom::Bloom(const DevicePtr &device, const Bloom::create_info_t &create_info)
{
    VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;

    vierkant::Framebuffer::create_info_t thresh_buffer_info = {};
    thresh_buffer_info.size = create_info.size;
    thresh_buffer_info.color_attachment_format.format = color_format;
    thresh_buffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    m_thresh_framebuffer = vierkant::Framebuffer(device, thresh_buffer_info);

    // create renderer for thresh-pass
    vierkant::Renderer::create_info_t thresh_render_info = {};
    thresh_render_info.num_frames_in_flight = 1;
    thresh_render_info.viewport.width = create_info.size.width;
    thresh_render_info.viewport.height = create_info.size.height;
    thresh_render_info.viewport.maxDepth = 1;
    thresh_render_info.pipeline_cache = create_info.pipeline_cache;
    m_thresh_renderer = vierkant::Renderer(device, thresh_render_info);

    // create gaussian blur
    GaussianBlur::create_info_t gaussian_info = {};
    gaussian_info.size = create_info.size;
    gaussian_info.color_format = color_format;
    gaussian_info.pipeline_cache = create_info.pipeline_cache;
    gaussian_info.num_iterations = create_info.num_blur_iterations;
    m_gaussian_blur = GaussianBlur::create(device, gaussian_info);


    m_drawable.pipeline_format.depth_test = false;
    m_drawable.pipeline_format.depth_write = false;

    m_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
    m_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::fullscreen::bloom_thresh_frag);
    m_drawable.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // set the specialization info
//    fmt.specialization_info = &m_specialization_info;

    // descriptor
    vierkant::descriptor_t desc_texture = {};
    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

    m_drawable.descriptors[0] = desc_texture;
    m_drawable.num_vertices = 3;
    m_drawable.use_own_buffers = true;
}

vierkant::ImagePtr Bloom::apply(const ImagePtr &image, VkQueue queue)
{
    if(!queue){ queue = image->device()->queue(); }

    // threshold
    m_drawable.descriptors[0].image_samplers = {image};
    m_thresh_renderer.stage_drawable(m_drawable);
    auto cmd_buf = m_thresh_renderer.render(m_thresh_framebuffer);
    m_thresh_framebuffer.submit({cmd_buf}, queue);

    // blur
    auto blur_img = m_gaussian_blur->apply(m_thresh_framebuffer.color_attachment(), queue);

    return blur_img;
}

}// namespace vierkant
