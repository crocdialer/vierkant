//
// Created by crocdialer on 1/3/22.
//

#include <vierkant/GBuffer.hpp>
#include <vierkant/shaders.hpp>

namespace vierkant
{
vierkant::Framebuffer create_g_buffer(const vierkant::DevicePtr &device,
                                      const VkExtent3D &extent,
                                      const vierkant::RenderPassPtr &renderpass)
{
    std::vector<vierkant::ImagePtr> g_buffer_attachments(G_BUFFER_SIZE);

    // albedo / emission / ao_rough_metal
    Image::Format color_format = {};
    color_format.extent = extent;
    color_format.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    g_buffer_attachments[G_BUFFER_ALBEDO] = vierkant::Image::create(device, color_format);
    g_buffer_attachments[G_BUFFER_EMISSION] = vierkant::Image::create(device, color_format);
    g_buffer_attachments[G_BUFFER_AO_ROUGH_METAL] = vierkant::Image::create(device, color_format);

    // normals
    Image::Format normal_format = {};
    normal_format.extent = extent;
    normal_format.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    normal_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    g_buffer_attachments[G_BUFFER_NORMAL] = vierkant::Image::create(device, normal_format);

    // motion
    Image::Format motion_format = {};
    motion_format.extent = extent;
    motion_format.format = VK_FORMAT_R16G16_SFLOAT;
    motion_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    g_buffer_attachments[G_BUFFER_MOTION] = vierkant::Image::create(device, motion_format);

    vierkant::Framebuffer::AttachmentMap attachments;

    attachments[Framebuffer::AttachmentType::Color] = g_buffer_attachments;

    // precision int24 vs float32 !?
    Image::Format depth_attachment_format = {};
    depth_attachment_format.extent = extent;
    depth_attachment_format.format = VK_FORMAT_D24_UNORM_S8_UINT;
    depth_attachment_format.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_attachment_format.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    attachments[Framebuffer::AttachmentType::DepthStencil] = {vierkant::Image::create(device, depth_attachment_format)};

    auto ret = vierkant::Framebuffer(device, attachments, renderpass);
    ret.clear_color = {{0.f, 0.f, 0.f, 0.f}};
    return ret;
}

g_buffer_stage_map_t create_g_buffer_shader_stages(const DevicePtr &device)
{
    g_buffer_stage_map_t ret;

    // vertex
    auto pbr_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_vert);
    auto pbr_skin_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_skin_vert);
    auto pbr_tangent_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_tangent_vert);
    auto pbr_tangent_skin_vert = vierkant::create_shader_module(device,
                                                                vierkant::shaders::pbr::g_buffer_tangent_skin_vert);

    // fragment
    auto pbr_g_buffer_frag = vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_frag);
    auto pbr_g_buffer_albedo_frag = vierkant::create_shader_module(device,
                                                                   vierkant::shaders::pbr::g_buffer_albedo_frag);
    auto pbr_g_buffer_albedo_normal_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_albedo_normal_frag);
    auto pbr_g_buffer_albedo_rough_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_albedo_rough_frag);
    auto pbr_g_buffer_albedo_normal_rough_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_albedo_normal_rough_frag);
    auto pbr_g_buffer_complete_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_complete_frag);

    auto pbr_g_buffer_uber_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::g_buffer_uber_frag);

    auto &stages_default = ret[PROP_DEFAULT];
    stages_default[VK_SHADER_STAGE_VERTEX_BIT] = pbr_vert;
    stages_default[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_frag;

    // albedo
    auto &stages_albedo = ret[PROP_ALBEDO];
    stages_albedo[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_albedo[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // skin
    auto &stages_skin = ret[PROP_SKIN];
    stages_skin[VK_SHADER_STAGE_VERTEX_BIT] = pbr_skin_vert;
    stages_skin[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // skin + albedo
    auto &stages_skin_albedo = ret[PROP_SKIN | PROP_ALBEDO];
    stages_skin_albedo[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_skin_vert;
    stages_skin_albedo[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // albedo + normals
    auto &stages_albedo_normal = ret[PROP_ALBEDO | PROP_NORMAL];
    stages_albedo_normal[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_albedo_normal[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // albedo + ao/rough/metal
    auto &stages_albedo_rough = ret[PROP_ALBEDO | PROP_AO_METAL_ROUGH];
    stages_albedo_rough[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_albedo_rough[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // albedo + normals + ao/rough/metal
    auto &stages_albedo_normal_rough = ret[PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH];
    stages_albedo_normal_rough[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_albedo_normal_rough[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // skin + albedo + normals + ao/rough/metal
    auto &stages_skin_albedo_normal_rough = ret[PROP_SKIN | PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH];
    stages_skin_albedo_normal_rough[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_skin_vert;
    stages_skin_albedo_normal_rough[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // albedo + normals + ao/rough/metal + emmission
    auto &stages_complete = ret[PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH | PROP_EMMISION];
    stages_complete[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_complete[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // skin + albedo + normals + ao/rough/metal + emmission
    auto &stages_skin_complete = ret[PROP_SKIN | PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH | PROP_EMMISION];
    stages_skin_complete[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_skin_vert;
    stages_skin_complete[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    return ret;
}

}