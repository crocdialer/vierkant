//
// Created by crocdialer on 1/3/22.
//

#include <vierkant/GBuffer.hpp>

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
}