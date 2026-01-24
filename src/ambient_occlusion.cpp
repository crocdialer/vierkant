#include <vierkant/Rasterizer.hpp>
#include <vierkant/ambient_occlusion.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/staging_copy.hpp>

namespace vierkant
{

struct ambient_occlusion_context_t
{
    vierkant::DevicePtr device;
    vierkant::PipelineCachePtr pipeline_cache;
    vierkant::BufferPtr param_buffer;
    vierkant::drawable_t drawable_ssao, drawable_rtao;
    vierkant::Framebuffer framebuffer;
    vierkant::Rasterizer renderer;
    std::default_random_engine random_engine;
};

struct alignas(16) ssao_params_t
{
    glm::mat4 projection{};
    glm::mat4 inverse_projection{};
    transform_t view_transform;
    float ssao_radius = 0.f;
    uint32_t random_seed = 0;
};

struct alignas(16) rtao_params_t
{
    glm::mat4 inverse_projection{};
    transform_t camera_transform;
    uint32_t num_rays{};
    float max_distance{};
};

ambient_occlusion_context_ptr create_ambient_occlusion_context(const vierkant::DevicePtr &device, const glm::vec2 &size,
                                                               const vierkant::PipelineCachePtr &pipeline_cache,
                                                               const vierkant::DescriptorPoolPtr &descriptor_pool)
{
    auto ret = ambient_occlusion_context_ptr(new ambient_occlusion_context_t,
                                             std::default_delete<ambient_occlusion_context_t>());
    ret->device = device;
    ret->pipeline_cache = pipeline_cache;

    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    framebuffer_info.color_attachment_format.format = VK_FORMAT_R16_SFLOAT;
    framebuffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    framebuffer_info.debug_label = {.text = "ambient_occlusion"};
    framebuffer_info.color_attachment_format.name = "ambient_occlusion";
    ret->framebuffer = vierkant::Framebuffer(device, framebuffer_info);

    // create renderer for thresh-pass
    vierkant::Rasterizer::create_info_t renderer_info = {};
    renderer_info.num_frames_in_flight = 1;
    renderer_info.viewport.width = size.x;
    renderer_info.viewport.height = size.y;
    renderer_info.viewport.maxDepth = 1;
    renderer_info.pipeline_cache = pipeline_cache;
    renderer_info.descriptor_pool = descriptor_pool;
    ret->renderer = vierkant::Rasterizer(device, renderer_info);

    vierkant::Buffer::create_info_t param_buffer_info = {};
    param_buffer_info.device = device;
    param_buffer_info.num_bytes = 1U << 10U;
    param_buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    param_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    param_buffer_info.name = "ambient_occlusion_params_buffer";
    ret->param_buffer = vierkant::Buffer::create(param_buffer_info);

    // ssao drawable
    auto ssao_vert = vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
    auto ssao_frag = vierkant::create_shader_module(device, vierkant::shaders::fullscreen::ao_screenspace_frag);
    auto ray_ao_frag = vierkant::create_shader_module(device, vierkant::shaders::fullscreen::ao_rayquery_frag);

    ret->drawable_ssao.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] = ssao_vert;
    ret->drawable_ssao.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] = ssao_frag;
    ret->drawable_ssao.pipeline_format.depth_test = false;
    ret->drawable_ssao.pipeline_format.depth_write = false;
    ret->drawable_ssao.num_vertices = 3;
    ret->drawable_ssao.use_own_buffers = true;

    // descriptors ssao
    auto &desc_params = ret->drawable_ssao.descriptors[0];
    desc_params.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_params.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    auto &desc_texture = ret->drawable_ssao.descriptors[1];
    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // rtao drawable
    ret->drawable_rtao = ret->drawable_ssao;
    auto &desc_top_lvl = ret->drawable_rtao.descriptors[0];
    desc_top_lvl.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    desc_top_lvl.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    auto &desc_texture_rtao = ret->drawable_rtao.descriptors[1];
    desc_texture_rtao.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture_rtao.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    auto &desc_params_rtao = ret->drawable_rtao.descriptors[2];
    desc_params_rtao.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_params_rtao.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ret->drawable_rtao.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] = ray_ao_frag;
    return ret;
}
vierkant::ImagePtr ambient_occlusion(const ambient_occlusion_context_ptr &context,
                                     const ambient_occlusion_params_t &params)
{
    assert(context);
    if(params.random_seed) { context->random_engine.seed(*params.random_seed); }

    // use ray-queries / RTAO if requested and available
    bool use_rtao = params.use_ray_queries && params.top_level;

    // debug label
    vierkant::begin_label(params.commandbuffer, {fmt::format("ambient_occlusion")});

    auto drawable = use_rtao ? context->drawable_rtao : context->drawable_ssao;

    void *ubo_ptr = context->param_buffer->map();
    assert(ubo_ptr);

    // RTAO
    if(use_rtao)
    {
        drawable.descriptors[0].acceleration_structures = {params.top_level};
        drawable.descriptors[1].images = {params.depth_img, params.normal_img};
        drawable.descriptors[2].buffers = {context->param_buffer};

        auto *rtao_params = static_cast<rtao_params_t *>(ubo_ptr);
        rtao_params->num_rays = params.num_rays;
        rtao_params->max_distance = params.max_distance;
        rtao_params->camera_transform = params.camera_transform;
        rtao_params->inverse_projection = glm::inverse(params.projection);
    }
    else// SSAO
    {
        drawable.descriptors[0].buffers = {context->param_buffer};
        drawable.descriptors[1].images = {params.depth_img, params.normal_img};

        auto *ssao_params = static_cast<ssao_params_t *>(ubo_ptr);
        ssao_params->projection = params.projection;
        ssao_params->inverse_projection = glm::inverse(params.projection);
        ssao_params->view_transform = vierkant::inverse(params.camera_transform);
        ssao_params->ssao_radius = params.max_distance;
        ssao_params->random_seed = context->random_engine();
    }

    auto ao_img = context->framebuffer.color_attachment();
    ao_img->transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, params.commandbuffer);

    vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
    begin_rendering_info.commandbuffer = params.commandbuffer;
    context->framebuffer.begin_rendering(begin_rendering_info);

    vierkant::Rasterizer::rendering_info_t rendering_info = {};
    rendering_info.command_buffer = params.commandbuffer;
    rendering_info.color_attachment_formats = {context->framebuffer.color_attachment()->format().format};

    context->renderer.stage_drawable(drawable);
    context->renderer.render(rendering_info);
    vkCmdEndRendering(params.commandbuffer);

    ao_img->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, params.commandbuffer, 0);
    vierkant::end_label(params.commandbuffer);
    return ao_img;
}

}// namespace vierkant