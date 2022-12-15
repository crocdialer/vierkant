//
// Created by crocdialer on 10/17/20.
//

#include "crocore/gaussian.hpp"
#include "vierkant/GaussianBlur.hpp"
#include "vierkant/shaders.hpp"

namespace vierkant
{

template
class GaussianBlur_<5>;

template
class GaussianBlur_<7>;

template
class GaussianBlur_<9>;

template
class GaussianBlur_<11>;

template
class GaussianBlur_<13>;

template<uint32_t NUM_TAPS>
std::unique_ptr<GaussianBlur_<NUM_TAPS>>
GaussianBlur_<NUM_TAPS>::create(const DevicePtr &device, const create_info_t &create_info)
{
    return std::unique_ptr<GaussianBlur_<NUM_TAPS>>(new GaussianBlur_<NUM_TAPS>(device, create_info));
}

template<uint32_t NUM_TAPS>
GaussianBlur_<NUM_TAPS>::GaussianBlur_(const DevicePtr &device, const create_info_t &create_info)
{
    m_command_pool = create_info.command_pool ?
                     create_info.command_pool : vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

//    m_command_buffer = vierkant::CommandBuffer(device, m_command_pool.get());

    // create renderer for blur-passes
    vierkant::Renderer::create_info_t post_render_info = {};
    post_render_info.num_frames_in_flight = 2 * create_info.num_iterations;
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport.width = create_info.size.width;
    post_render_info.viewport.height = create_info.size.height;
    post_render_info.viewport.maxDepth = 1;
    post_render_info.pipeline_cache = create_info.pipeline_cache;
    post_render_info.command_pool = m_command_pool;
    post_render_info.descriptor_pool = create_info.descriptor_pool;
    m_renderer = vierkant::Renderer(device, post_render_info);

    // init framebuffers
    vierkant::attachment_map_t fb_attachments_ping, fb_attachments_pong;
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_fmt.format = create_info.color_format;
    img_fmt.extent = create_info.size;

    fb_attachments_ping[vierkant::AttachmentType::Color] = {vierkant::Image::create(device, img_fmt)};
    fb_attachments_pong[vierkant::AttachmentType::Color] = {vierkant::Image::create(device, img_fmt)};

    vierkant::RenderPassPtr renderpass;
    renderpass = vierkant::create_renderpass(device, fb_attachments_ping, true, false);

    m_framebuffers.resize(2 * create_info.num_iterations);

    for(uint32_t i = 0; i < m_framebuffers.size(); i += 2)
    {
        m_framebuffers[i] = vierkant::Framebuffer(device, fb_attachments_ping, renderpass);
        m_framebuffers[i + 1] = vierkant::Framebuffer(device, fb_attachments_pong, renderpass);
    }

    // symmetric 1D gaussian kernels
    auto gaussian_x = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.x);
    auto gaussian_y = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.y);

    auto size = glm::vec2(create_info.size.width, create_info.size.height);

    auto create_weights = [&size](const auto &kernel, bool horizontal) -> ubo_t
    {
        // [-NUM_TAPS / 2, ..., NUM_TAPS / 2]
        std::array<float, NUM_TAPS> offsets;
        std::iota(offsets.begin(), offsets.end(), -static_cast<float>(NUM_TAPS / 2));

        ubo_t ubo = {};
        ubo.size = size;

        // modify weights locally
        auto kernel_tmp = kernel;

        // odd or even number of samples
        bool odd_num_samples = (num_taps / 2) % 2 == 0;

        if(odd_num_samples)
        {
            ubo.offsets[0] = glm::vec4(0.f);
            ubo.weights[0] = glm::vec4(kernel[kernel.size() / 2]);
        }
        else
        {
            // center weight will be used twice
            kernel_tmp[kernel.size() / 2] /= 2.f;
        }

        uint32_t k = odd_num_samples ? 1 : 0;

        for(uint32_t i = kernel_tmp.size() / 2 + k; i < kernel_tmp.size(); i += 2)
        {
            float weight_sum = kernel_tmp[i] + kernel_tmp[i + 1];
            ubo.weights[k] = glm::vec4(weight_sum);

            float offset = offsets[i] * kernel_tmp[i] + offsets[i + 1] * kernel_tmp[i + 1];
            offset /= weight_sum;

            // set 2d offset
            glm::vec2 offset_h = glm::vec2(offset, 0.f);
            ubo.offsets[k] = glm::vec4(horizontal ? offset_h : offset_h.yx(), 0.f, 0.f);

            k++;
        }
        return ubo;
    };

    ubo_t ubos[2];

    // horizontal + vertical offsets and weights
    ubos[0] = create_weights(gaussian_x, true);
    ubos[1] = create_weights(gaussian_y, false);

    m_ping_pongs[0].ubo = vierkant::Buffer::create(device, &ubos[0], sizeof(ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VMA_MEMORY_USAGE_CPU_TO_GPU);

    m_ping_pongs[1].ubo = vierkant::Buffer::create(device, &ubos[1], sizeof(ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VMA_MEMORY_USAGE_CPU_TO_GPU);

    // create drawable for post-fx-pass
    {
        m_specialization_entry.constantID = 0;
        m_specialization_entry.offset = 0;
        m_specialization_entry.size = sizeof(num_taps);

        m_specialization_info.mapEntryCount = 1;
        m_specialization_info.pMapEntries = &m_specialization_entry;
        m_specialization_info.pData = &num_taps;
        m_specialization_info.dataSize = sizeof(num_taps);

        vierkant::drawable_t drawable = {};

        graphics_pipeline_info_t fmt = {};
        fmt.depth_test = false;
        fmt.depth_write = false;

        fmt.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
        fmt.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::gaussian_blur_frag);
        fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // set the specialization info
        fmt.specialization_info = &m_specialization_info;

        // descriptor
        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        drawable.descriptors[0] = desc_texture;
        drawable.num_vertices = 3;
        drawable.pipeline_format = fmt;
        drawable.use_own_buffers = true;

        // descriptor
        vierkant::descriptor_t desc_settings_ubo = {};
        desc_settings_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_settings_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        drawable.descriptors[1] = std::move(desc_settings_ubo);

        m_ping_pongs[0].drawable = drawable;
        m_ping_pongs[0].drawable.descriptors[1].buffers = {m_ping_pongs[0].ubo};

        m_ping_pongs[1].drawable = drawable;
        m_ping_pongs[1].drawable.descriptors[1].buffers = {m_ping_pongs[1].ubo};
    }
}

template<uint32_t NUM_TAPS>
vierkant::ImagePtr GaussianBlur_<NUM_TAPS>::apply(const ImagePtr &image, VkQueue queue,
                                                  const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos)
{
    auto device = image->device();

    if(!queue){ queue = device->queue(); }

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

    // init/reset semaphore
    m_semaphore.wait(m_framebuffers.size() - 1);
    m_semaphore = vierkant::Semaphore(device);
    vierkant::semaphore_submit_info_t semaphore_info_ping = {}, semaphore_info_pong = {};
    semaphore_info_ping.semaphore = semaphore_info_pong.semaphore = m_semaphore.handle();

    auto current_img = image;
    auto &ping = m_ping_pongs[0], &pong = m_ping_pongs[1];

    // wait for external semaphores
    vierkant::submit(device, queue, {}, false, VK_NULL_HANDLE, wait_infos);

    for(uint32_t i = 0; i < m_framebuffers.size(); i += 2)
    {
        semaphore_info_ping.wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        semaphore_info_ping.wait_value = i;
        semaphore_info_ping.signal_value = i + 1;

        semaphore_info_pong.wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        semaphore_info_pong.wait_value = i + 1;
        semaphore_info_pong.signal_value = i + 2;

        auto &fb_ping = m_framebuffers[i];
        auto &fb_pong = m_framebuffers[i + 1];

        ping.drawable.descriptors[0].images = {current_img};
        pong.drawable.descriptors[0].images = {fb_ping.color_attachment()};

        // horizontal pass
        m_renderer.stage_drawable(ping.drawable);
        auto cmd_buffer = m_renderer.render(fb_ping);
        fb_ping.submit({cmd_buffer}, queue, {semaphore_info_ping});

        // vertical pass
        m_renderer.stage_drawable(pong.drawable);
        cmd_buffer = m_renderer.render(fb_pong);
        fb_pong.submit({cmd_buffer}, queue, {semaphore_info_pong});

        current_img = fb_pong.color_attachment();
    }

    // wait on pingpongs, signal external semaphores
    vierkant::semaphore_submit_info_t semaphore_info_final = {m_semaphore.handle()};
    semaphore_info_final.wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    semaphore_info_final.wait_value = m_framebuffers.size();
    signal_infos.push_back(semaphore_info_final);

    if(!m_command_buffer)
    {
        m_command_buffer = vierkant::CommandBuffer(device, m_command_pool.get());
        m_command_buffer.begin(0);
        m_framebuffers.back().color_attachment()->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                                    m_command_buffer.handle());
        m_command_buffer.end();
    }

    vierkant::submit(device, queue, {m_command_buffer.handle()}, false, VK_NULL_HANDLE, signal_infos);

    return current_img;
}

}// namespace vierkant