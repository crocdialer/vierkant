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

Bloom::Bloom(const DevicePtr &device, const Bloom::create_info_t &create_info) :
        m_brightness_thresh(create_info.brightness_thresh)
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
    thresh_render_info.viewport.width = static_cast<float>(create_info.size.width);
    thresh_render_info.viewport.height = static_cast<float>(create_info.size.height);
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
    if(!queue){ queue = image->device()->queue(); }

    m_semaphore.wait(SemaphoreValue::BLUR_DONE);
    m_semaphore = vierkant::Semaphore(image->device());

    std::vector<vierkant::semaphore_submit_info_t> wait_infos, signal_infos;

    for(const auto &info: semaphore_infos)
    {
        if(info.semaphore)
        {
            if(info.signal_value)
            {
                auto signal_info = info;
                signal_info.wait_stage = 0;
                signal_info.wait_value = 0;
                signal_infos.push_back(signal_info);
            }
            if(info.wait_stage)
            {
                auto wait_info = info;
                wait_info.signal_value = 0;
                wait_infos.push_back(wait_info);
            }
        }
    }

    vierkant::semaphore_submit_info_t thresh_done = {};
    thresh_done.semaphore = m_semaphore.handle();
    thresh_done.signal_value = SemaphoreValue::THRESH_DONE;
    thresh_done.signal_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    // threshold
    auto thresh_submit_infos = wait_infos;
    thresh_submit_infos.push_back(thresh_done);

    m_drawable.descriptors[0].images = {image};
    m_thresh_renderer.stage_drawable(m_drawable);
    auto cmd_buf = m_thresh_renderer.render(m_thresh_framebuffer);
    m_thresh_framebuffer.submit({cmd_buf}, queue, thresh_submit_infos);

    // blur
    vierkant::semaphore_submit_info_t blur_info = {};
    blur_info.semaphore = m_semaphore.handle();
    blur_info.wait_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    blur_info.wait_value = SemaphoreValue::THRESH_DONE;
    blur_info.signal_value = SemaphoreValue::BLUR_DONE;
    blur_info.signal_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    auto blur_submit_infos = signal_infos;
    blur_submit_infos.push_back(blur_info);
    return m_gaussian_blur->apply(m_thresh_framebuffer.color_attachment(), queue, blur_submit_infos);
}

}// namespace vierkant
