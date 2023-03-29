//
// Created by crocdialer on 10/28/20.
//

#include <vierkant/Bloom.hpp>
#include <vierkant/shaders.hpp>

namespace vierkant
{

BloomUPtr Bloom::create(const DevicePtr &device, const Bloom::create_info_t &create_info)
{
    return vierkant::BloomUPtr(new Bloom(device, create_info));
}

Bloom::Bloom(const DevicePtr &device, const Bloom::create_info_t &create_info)
    : m_device(device), m_brightness_thresh(create_info.brightness_thresh)
{
    m_command_pool = create_info.command_pool;

    if(!m_command_pool)
    {
        m_command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                       VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    }

    vierkant::Framebuffer::create_info_t thresh_buffer_info = {};
    thresh_buffer_info.size = create_info.size;
    thresh_buffer_info.color_attachment_format.format = create_info.color_format;
    thresh_buffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    m_thresh_framebuffer = vierkant::Framebuffer(device, thresh_buffer_info);

    // create renderer for thresh-pass
    vierkant::Renderer::create_info_t thresh_render_info = {};
    thresh_render_info.num_frames_in_flight = 1;
    thresh_render_info.viewport.width = static_cast<float>(create_info.size.width);
    thresh_render_info.viewport.height = static_cast<float>(create_info.size.height);
    thresh_render_info.viewport.maxDepth = 1;
    thresh_render_info.pipeline_cache = create_info.pipeline_cache;
    thresh_render_info.descriptor_pool = create_info.descriptor_pool;
    thresh_render_info.command_pool = m_command_pool;
    m_thresh_renderer = vierkant::Renderer(device, thresh_render_info);

    m_command_buffer = vierkant::CommandBuffer(device, m_command_pool.get());

    // create gaussian blur
    GaussianBlur::create_info_t gaussian_info = {};
    gaussian_info.size = create_info.size;
    gaussian_info.color_format = create_info.color_format;
    gaussian_info.pipeline_cache = create_info.pipeline_cache;
    gaussian_info.command_pool = m_command_pool;
    gaussian_info.descriptor_pool = create_info.descriptor_pool;
    gaussian_info.num_iterations = create_info.num_blur_iterations;
    m_gaussian_blur = GaussianBlur::create(device, gaussian_info);

    // threshold drawable
    m_drawable.pipeline_format.depth_test = false;
    m_drawable.pipeline_format.depth_write = false;

    m_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
    m_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::fullscreen::bloom_thresh_frag);
    m_drawable.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // set the specialization info
    m_specialization_entry[0].constantID = 0;
    m_specialization_entry[0].offset = 0;
    m_specialization_entry[0].size = sizeof(float);
    m_specialization_entry[1].constantID = 1;
    m_specialization_entry[1].offset = offsetof(glm::vec2, y);
    m_specialization_entry[1].size = sizeof(float);

    m_specialization_info.mapEntryCount = 2;
    m_specialization_info.pMapEntries = m_specialization_entry.data();
    m_specialization_info.pData = glm::value_ptr(m_brightness_thresh);
    m_specialization_info.dataSize = sizeof(glm::vec2);
    m_drawable.pipeline_format.specialization_info = &m_specialization_info;

    // descriptor
    vierkant::descriptor_t desc_texture = {};
    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

    m_drawable.descriptors[0] = desc_texture;
    m_drawable.num_vertices = 3;
    m_drawable.use_own_buffers = true;
}

vierkant::ImagePtr Bloom::apply(const ImagePtr &image, VkQueue queue,
                                const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos)
{
    m_command_buffer.begin(0);
    auto ret = apply(image, m_command_buffer.handle());
    m_command_buffer.submit(queue, false, VK_NULL_HANDLE, semaphore_infos);
    return ret;
}

vierkant::ImagePtr Bloom::apply(const ImagePtr &image, VkCommandBuffer commandbuffer)
{
    // debug label
    m_device->begin_label(commandbuffer, fmt::format("Bloom::apply"));

    // threshold
    vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
    begin_rendering_info.commandbuffer = commandbuffer;
    m_thresh_framebuffer.begin_rendering(begin_rendering_info);

    vierkant::Renderer::rendering_info_t rendering_info = {};
    rendering_info.command_buffer = commandbuffer;
    rendering_info.color_attachment_formats = {m_thresh_framebuffer.color_attachment()->format().format};

    m_drawable.descriptors[0].images = {image};
    m_thresh_renderer.stage_drawable(m_drawable);
    m_thresh_renderer.render(rendering_info);
    vkCmdEndRendering(commandbuffer);

    // blur
    auto blur = m_gaussian_blur->apply(m_thresh_framebuffer.color_attachment(), commandbuffer);
    m_device->end_label(commandbuffer);
    return blur;
}

}// namespace vierkant
