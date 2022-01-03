//
// Created by crocdialer on 1/3/22.
//

#pragma once

#include <vierkant/Framebuffer.hpp>

namespace vierkant
{

enum G_BUFFER
{
    G_BUFFER_ALBEDO = 0,
    G_BUFFER_NORMAL = 1,
    G_BUFFER_EMISSION = 2,
    G_BUFFER_AO_ROUGH_METAL = 3,
    G_BUFFER_MOTION = 4,
    G_BUFFER_SIZE = 5
};

/**
 * @brief   create_g_buffer can be used to create a vierkant::Framebuffer usable as G-Buffer.
 *          Attachments correspond to above enum.
 *
 * @param   device      handle to a vierkant::Device to create the framebuffer.
 * @param   extent      desired framebuffer-extent.
 * @param   renderpass  optional renderpass to use for framebuffer-creation.
 * @return  a vierkant::Framebuffer representing a g-buffer
 */
vierkant::Framebuffer create_g_buffer(const DevicePtr &device,
                                      const VkExtent3D &extent,
                                      const vierkant::RenderPassPtr &renderpass = nullptr);
}// namespace vierkant
