#include "vierkant/GaussianBlur.hpp"
#include "crocore/gaussian.hpp"
#include "vierkant/shaders.hpp"

namespace vierkant
{

template<uint32_t NUM_TAPS>
std::unique_ptr<GaussianBlur_<NUM_TAPS>> GaussianBlur_<NUM_TAPS>::create(const DevicePtr &device,
                                                                         const create_info_t &create_info)
{
    return std::unique_ptr<GaussianBlur_<NUM_TAPS>>(new GaussianBlur_<NUM_TAPS>(device, create_info));
}

template<uint32_t NUM_TAPS>
GaussianBlur_<NUM_TAPS>::GaussianBlur_(const DevicePtr &device, const create_info_t &create_info)
{
    m_device = device;
    m_num_iterations = create_info.num_iterations;
    m_color_format = create_info.color_format;

    auto command_pool = create_info.command_pool;

    if(!command_pool)
    {
        command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                     VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                             VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    }

    // create renderer for blur-passes
    vierkant::Rasterizer::create_info_t post_render_info = {};
    post_render_info.indirect_draw = false;
    post_render_info.num_frames_in_flight = 2 * create_info.num_iterations;
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport.width = static_cast<float>(create_info.size.width);
    post_render_info.viewport.height = static_cast<float>(create_info.size.height);
    post_render_info.viewport.maxDepth = 1;
    post_render_info.pipeline_cache = create_info.pipeline_cache;
    post_render_info.command_pool = command_pool;
    post_render_info.descriptor_pool = create_info.descriptor_pool;
    m_renderer = vierkant::Rasterizer(device, post_render_info);

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

    m_ping_pongs[0].framebuffer = vierkant::Framebuffer(device, fb_attachments_ping, renderpass);
    m_ping_pongs[1].framebuffer = vierkant::Framebuffer(device, fb_attachments_pong, renderpass);

    m_command_buffer = vierkant::CommandBuffer(device, command_pool.get());

    // symmetric 1D gaussian kernels
    auto gaussian_x = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.x);
    auto gaussian_y = crocore::createGaussianKernel_1D<NUM_TAPS>(create_info.sigma.y);

    auto size = glm::vec2(create_info.size.width, create_info.size.height);

    auto create_weights = [&size](const auto &kernel, bool horizontal) -> ubo_t {
        // [-NUM_TAPS / 2, ..., NUM_TAPS / 2]
        std::array<float, NUM_TAPS> offsets;
        std::iota(offsets.begin(), offsets.end(), -static_cast<float>(NUM_TAPS / 2.f));

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
        vierkant::pipeline_specialization pipeline_specialization;
        pipeline_specialization.set(0, num_taps);
        fmt.specialization = std::move(pipeline_specialization);

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
    m_command_buffer.begin(0);
    auto ret = apply(image, m_command_buffer.handle());
    m_command_buffer.submit(queue, false, VK_NULL_HANDLE, semaphore_infos);
    return ret;
}

template<uint32_t NUM_TAPS>
vierkant::ImagePtr GaussianBlur_<NUM_TAPS>::apply(const ImagePtr &image, VkCommandBuffer commandbuffer)
{
    auto current_img = image;
    auto &ping = m_ping_pongs[0], &pong = m_ping_pongs[1];

    // debug label
    vierkant::begin_label(commandbuffer, {fmt::format("GaussianBlur_<{}>::apply>", NUM_TAPS)});

    vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
    begin_rendering_info.commandbuffer = commandbuffer;

    for(uint32_t i = 0; i < m_num_iterations; ++i)
    {
        ping.drawable.descriptors[0].images = {current_img};
        pong.drawable.descriptors[0].images = {ping.framebuffer.color_attachment()};

        vierkant::Rasterizer::rendering_info_t rendering_info = {};
        rendering_info.command_buffer = commandbuffer;
        rendering_info.color_attachment_formats = {m_color_format};

        // horizontal pass
        m_renderer.stage_drawable(ping.drawable);
        current_img->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, commandbuffer);
        ping.framebuffer.color_attachment()->transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, commandbuffer);
        ping.framebuffer.begin_rendering(begin_rendering_info);
        m_renderer.render(rendering_info);
        vkCmdEndRendering(commandbuffer);

        // vertical pass
        m_renderer.stage_drawable(pong.drawable);
        ping.framebuffer.color_attachment()->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, commandbuffer);
        pong.framebuffer.color_attachment()->transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, commandbuffer);
        pong.framebuffer.begin_rendering(begin_rendering_info);
        m_renderer.render(rendering_info);
        vkCmdEndRendering(commandbuffer);
        current_img = pong.framebuffer.color_attachment();
    }
    current_img->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, commandbuffer);

    vierkant::end_label(commandbuffer);
    return current_img;
}

template class GaussianBlur_<5>;
template class GaussianBlur_<7>;
template class GaussianBlur_<9>;
template class GaussianBlur_<11>;
template class GaussianBlur_<13>;

}// namespace vierkant
