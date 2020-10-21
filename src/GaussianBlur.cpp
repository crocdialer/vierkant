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
class GaussianBlur_<9>;

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
    // create renderer for blur-passes
    vierkant::Renderer::create_info_t post_render_info = {};
    post_render_info.num_frames_in_flight = 2;
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport.width = create_info.size.width;
    post_render_info.viewport.height = create_info.size.height;
    post_render_info.viewport.maxDepth = 1;
    post_render_info.pipeline_cache = create_info.pipeline_cache;
    m_renderer = vierkant::Renderer(device, post_render_info);

    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = create_info.size;
    framebuffer_info.color_attachment_format.format = create_info.color_format;
    framebuffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    m_ping_pongs[0].framebuffer = vierkant::Framebuffer(device, framebuffer_info);
    m_ping_pongs[1].framebuffer = vierkant::Framebuffer(device, framebuffer_info);

    // symmetric 1D gaussian kernels
    auto gaussian_x = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.x);
    auto gaussian_y = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.y);

    auto create_weights = [](const auto &kernel, bool horizontal) -> ubo_t
    {
        // [-NUM_TAPS / 2, ..., NUM_TAPS / 2]
        std::array<float, NUM_TAPS> offsets;
        std::iota(offsets.begin(), offsets.end(), -static_cast<float>(NUM_TAPS / 2));

        ubo_t ubo = {};

        uint32_t k = 1;
        ubo.offsets[0] = glm::vec4(0.f);
        ubo.weights[0] = glm::vec4(kernel[kernel.size() / 2]);

        for(uint32_t i = kernel.size() / 2 + 1; i < kernel.size(); i += 2)
        {
            float weight_sum = kernel[i] + kernel[i + 1];
            ubo.weights[k] = glm::vec4(weight_sum);

            float offset = offsets[i] * kernel[i] + offsets[i + 1] * kernel[i + 1];
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
        m_specialization_entry.size = sizeof(ubo_array_size);

        m_specialization_info.mapEntryCount = 1;
        m_specialization_info.pMapEntries = &m_specialization_entry;
        m_specialization_info.pData = &ubo_array_size;
        m_specialization_info.dataSize = sizeof(ubo_array_size);

        vierkant::Renderer::drawable_t drawable = {};

        Pipeline::Format fmt = {};
        fmt.depth_test = false;
        fmt.depth_write = false;

        fmt.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen_texture_vert);
        fmt.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen_gaussian_blur_frag);
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
        m_ping_pongs[0].drawable.descriptors[1].buffer = m_ping_pongs[0].ubo;

        m_ping_pongs[1].drawable = drawable;
        m_ping_pongs[1].drawable.descriptors[1].buffer = m_ping_pongs[1].ubo;
    }
}

template<uint32_t NUM_TAPS>
vierkant::ImagePtr GaussianBlur_<NUM_TAPS>::apply(const ImagePtr &image, VkQueue queue)
{
    if(!queue){ queue = image->device()->queue(); }

    auto &ping = m_ping_pongs[0], &pong = m_ping_pongs[1];

    ping.drawable.descriptors[0].image_samplers = {image};
    pong.drawable.descriptors[0].image_samplers = {ping.framebuffer.color_attachment()};

    // horizontal pass
    m_renderer.stage_drawable(ping.drawable);
    auto cmd_buffer = m_renderer.render(ping.framebuffer);
    ping.framebuffer.submit({cmd_buffer}, queue);

    // vertical pass
    m_renderer.stage_drawable(pong.drawable);
    cmd_buffer = m_renderer.render(pong.framebuffer);
    pong.framebuffer.submit({cmd_buffer}, queue);

    return pong.framebuffer.color_attachment();
}

}// namespace vierkant