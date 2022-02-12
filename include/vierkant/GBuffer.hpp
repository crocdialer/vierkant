//
// Created by crocdialer on 1/3/22.
//

#pragma once

#include <vierkant/Framebuffer.hpp>
#include <vierkant/pipeline_formats.hpp>

namespace vierkant
{

//! GBuffer is an enum used to index g-buffer attachments
enum GBuffer : uint32_t
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
 *
 * @return  a vierkant::Framebuffer representing a g-buffer
 */
vierkant::Framebuffer create_g_buffer(const DevicePtr &device,
                                      const VkExtent3D &extent,
                                      const vierkant::RenderPassPtr &renderpass = nullptr);

enum GBufferPropertyFlagBits : uint32_t
{
    PROP_DEFAULT = 0x00,
    PROP_SKIN = 0x02,
    PROP_TANGENT_SPACE = 0x04,
};

using GBufferPropertyFlags = uint32_t;
using g_buffer_stage_map_t = std::unordered_map<GBufferPropertyFlags, vierkant::shader_stage_map_t>;

/**
 * @brief   create_g_buffer_shader_stages can be used to create a lookup-table for g-buffer shader-stages.
 *          the lookup-table contains all possible permutations of the GBufferPropertyFlagBits enum.
 *
 * @param   device  handle to a vierkant::Device to create the shader-stages.
 *
 * @return  a map containing shader-stages.
 */
g_buffer_stage_map_t create_g_buffer_shader_stages(const DevicePtr &device);

}// namespace vierkant
