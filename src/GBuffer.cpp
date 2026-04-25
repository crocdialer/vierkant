//
// Created by crocdialer on 1/3/22.
//

#include <vierkant/GBuffer.hpp>

#include <vierkant/shaders.hpp>
#include <vierkant/shaders_slang.hpp>

namespace vierkant
{
vierkant::Framebuffer create_g_buffer(const vierkant::DevicePtr &device, const VkExtent3D &extent)
{
    std::vector<vierkant::ImagePtr> g_buffer_attachments(G_BUFFER_SIZE);

    // albedo / ao_rough_metal
    Image::Format color_format = {};
    color_format.extent = extent;
    color_format.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    color_format.initial_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    color_format.name = "albedo";
    g_buffer_attachments[G_BUFFER_ALBEDO] = vierkant::Image::create(device, color_format);
    color_format.name = "ao_rough_metal";
    g_buffer_attachments[G_BUFFER_AO_ROUGH_METAL] = vierkant::Image::create(device, color_format);

    // emission
    Image::Format emission_format = {};
    emission_format.extent = extent;
    emission_format.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    emission_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    emission_format.initial_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    emission_format.name = "emission";
    g_buffer_attachments[G_BUFFER_EMISSION] = vierkant::Image::create(device, emission_format);

    // normals
    Image::Format normal_format = {};
    normal_format.extent = extent;
    normal_format.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    normal_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    normal_format.initial_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    normal_format.name = "normals";
    g_buffer_attachments[G_BUFFER_NORMAL] = vierkant::Image::create(device, normal_format);

    // motion
    Image::Format motion_format = {};
    motion_format.extent = extent;
    motion_format.format = VK_FORMAT_R16G16_SFLOAT;
    motion_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    motion_format.name = "motion";
    g_buffer_attachments[G_BUFFER_MOTION] = vierkant::Image::create(device, motion_format);

    // object-id
    Image::Format object_id_format = {};
    object_id_format.extent = extent;
    object_id_format.format = VK_FORMAT_R16_UINT;
    object_id_format.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    object_id_format.name = "object-id";
    g_buffer_attachments[G_BUFFER_OBJECT_ID] = vierkant::Image::create(device, object_id_format);

    vierkant::attachment_map_t attachments;

    attachments[AttachmentType::Color] = g_buffer_attachments;

    // precision int24 vs float32 !?
    Image::Format depth_attachment_format = {};
    depth_attachment_format.extent = extent;
    depth_attachment_format.format = VK_FORMAT_D32_SFLOAT;
    depth_attachment_format.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_attachment_format.usage =
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    depth_attachment_format.initial_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depth_attachment_format.name = "depth";
    attachments[AttachmentType::DepthStencil] = {vierkant::Image::create(device, depth_attachment_format)};

    auto ret = vierkant::Framebuffer(device, attachments, {});
    ret.clear_color = glm::vec4{0.f};
    return ret;
}

g_buffer_stage_map_t create_g_buffer_shader_stages(const DevicePtr & /*device*/)
{
    g_buffer_stage_map_t ret;

    // vertex
    auto pbr_tangent_vert = vierkant::create_shader_module(vierkant::slang_shaders::pbr::g_buffer_tangent_slang);

    // task / mesh
    auto pbr_tangent_task = vierkant::create_shader_module(vierkant::slang_shaders::pbr::g_buffer_task_slang);
    auto pbr_tangent_mesh = vierkant::create_shader_module(vierkant::slang_shaders::pbr::g_buffer_mesh_slang);

    // fragment
    auto pbr_g_buffer_uber_frag = vierkant::create_shader_module(vierkant::slang_shaders::pbr::g_buffer_uber_slang);

    auto &stages_default = ret[PROP_DEFAULT];
    stages_default[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_default[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;

    // meshlet pipelines
    auto &stages_mesh = ret[PROP_MESHLETS];
    stages_mesh[VK_SHADER_STAGE_TASK_BIT_EXT] = pbr_tangent_task;
    stages_mesh[VK_SHADER_STAGE_MESH_BIT_EXT] = pbr_tangent_mesh;
    stages_mesh[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_uber_frag;
    return ret;
}

}// namespace vierkant