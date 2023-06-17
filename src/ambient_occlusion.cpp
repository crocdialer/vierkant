//
// Created by crocdialer on 13.04.23.
//

#include <vierkant/Renderer.hpp>
#include <vierkant/ambient_occlusion.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/staging_copy.hpp>

namespace vierkant
{

struct ambient_occlusion_context_t
{
    vierkant::DevicePtr device;
    vierkant::CommandPoolPtr command_pool;
    vierkant::PipelineCachePtr pipeline_cache;
    vierkant::CommandBuffer cmd_buffer;
    vierkant::BufferPtr param_buffer;
    vierkant::BufferPtr staging_buffer;
    vierkant::drawable_t drawable_ssao, drawable_rtao;
    vierkant::Framebuffer framebuffer;
    vierkant::Renderer renderer;
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
                                                               const vierkant::PipelineCachePtr &pipeline_cache)
{
    auto ret = ambient_occlusion_context_ptr(new ambient_occlusion_context_t,
                                             std::default_delete<ambient_occlusion_context_t>());
    ret->device = device;
    ret->pipeline_cache = pipeline_cache;
    ret->command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    ret->cmd_buffer = vierkant::CommandBuffer(device, ret->command_pool.get());

    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    framebuffer_info.color_attachment_format.format = VK_FORMAT_R16_SFLOAT;
    framebuffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    framebuffer_info.debug_label = {"ambient_occlusion"};
    framebuffer_info.color_attachment_format.name = "ambient_occlusion";
    ret->framebuffer = vierkant::Framebuffer(device, framebuffer_info);

    // create renderer for thresh-pass
    vierkant::Renderer::create_info_t renderer_info = {};
    renderer_info.num_frames_in_flight = 1;
    renderer_info.viewport.width = size.x;
    renderer_info.viewport.height = size.y;
    renderer_info.viewport.maxDepth = 1;
    renderer_info.pipeline_cache = pipeline_cache;
    renderer_info.descriptor_pool = nullptr;
    renderer_info.command_pool = ret->command_pool;
    ret->renderer = vierkant::Renderer(device, renderer_info);

    vierkant::Buffer::create_info_t internal_buffer_info = {};
    internal_buffer_info.device = device;
    internal_buffer_info.num_bytes = 1U << 10U;
    internal_buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    internal_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;

    ret->param_buffer = vierkant::Buffer::create(internal_buffer_info);

    vierkant::Buffer::create_info_t staging_buffer_info = {};
    staging_buffer_info.device = device;
    staging_buffer_info.num_bytes = 1U << 10U;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_ONLY;
    ret->staging_buffer = vierkant::Buffer::create(staging_buffer_info);

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

    // debug label
    context->device->begin_label(params.commandbuffer, {fmt::format("ambient_occlusion")});

    auto drawable = params.top_level ? context->drawable_rtao : context->drawable_ssao;
    std::vector<vierkant::staging_copy_info_t> staging_copy_infos;

    // RTAO
    if(params.top_level)
    {
        drawable.descriptors[0].acceleration_structure = params.top_level;
        drawable.descriptors[1].images = {params.depth_img, params.normal_img};
        drawable.descriptors[2].buffers = {context->param_buffer};

        rtao_params_t rtao_params = {};
        rtao_params.num_rays = params.num_rays;
        rtao_params.max_distance = params.max_distance;
        rtao_params.camera_transform = params.camera_transform;
        rtao_params.inverse_projection = glm::inverse(params.projection);

        vierkant::staging_copy_info_t copy_params = {};
        copy_params.num_bytes = sizeof(rtao_params_t);
        copy_params.data = &rtao_params;
        copy_params.dst_buffer = context->param_buffer;
        copy_params.dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        copy_params.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
        staging_copy_infos.push_back(copy_params);
    }
    else// SSAO
    {
        drawable.descriptors[0].buffers = {context->param_buffer};
        drawable.descriptors[1].images = {params.depth_img, params.normal_img};

        ssao_params_t ssao_params = {};
        ssao_params.projection = params.projection;
        ssao_params.inverse_projection = glm::inverse(params.projection);
        ssao_params.view_transform = vierkant::inverse(params.camera_transform);
        ssao_params.ssao_radius = params.max_distance;
        ssao_params.random_seed = context->random_engine();

        vierkant::staging_copy_info_t copy_params = {};
        copy_params.num_bytes = sizeof(ssao_params_t);
        copy_params.data = &ssao_params;
        copy_params.dst_buffer = context->param_buffer;
        copy_params.dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        copy_params.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
        staging_copy_infos.push_back(copy_params);
    }

    // ubo upload
    vierkant::staging_copy_context_t staging_context = {};
    staging_context.command_buffer = params.commandbuffer;
    staging_context.staging_buffer = context->staging_buffer;
    vierkant::staging_copy(staging_context, staging_copy_infos);

    auto ao_img = context->framebuffer.color_attachment();
    ao_img->transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, params.commandbuffer);

    vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
    begin_rendering_info.commandbuffer = params.commandbuffer;
    context->framebuffer.begin_rendering(begin_rendering_info);

    vierkant::Renderer::rendering_info_t rendering_info = {};
    rendering_info.command_buffer = params.commandbuffer;
    rendering_info.color_attachment_formats = {context->framebuffer.color_attachment()->format().format};

    context->renderer.stage_drawable(drawable);
    context->renderer.render(rendering_info);
    vkCmdEndRendering(params.commandbuffer);

    ao_img->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, params.commandbuffer, 0);
    context->device->end_label(params.commandbuffer);
    return ao_img;
}

}// namespace vierkant