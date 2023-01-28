//
// Created by crocdialer on 10.01.23.
//

#pragma once

#include <vierkant/Buffer.hpp>

namespace vierkant
{

//! staging_copy_info_t groups information for an individual staging-copy.
struct staging_copy_info_t
{
    const void *data = nullptr;
    size_t num_bytes = 0;
    vierkant::BufferPtr dst_buffer;
    size_t dst_offset = 0;
    VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 dst_access = VK_ACCESS_2_NONE;
};

//! staging_copy_context_t provides a context for a serious of staging-copies.
struct staging_copy_context_t
{
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    vierkant::BufferPtr staging_buffer;
    size_t offset = 0;
};

/**
 * @brief   staging_copy can be used to schedule a list of staging-copies,
 *          using a provided staging-context.
 *
 * @param   context             a provided context for stagin-copies
 * @param   staging_copy_infos  an array of copy-infos
 * @return  total number of bytes (size of staging-buffer | context's current staging-offset)
 */
size_t staging_copy(staging_copy_context_t &context,
                    const std::vector<staging_copy_info_t> &staging_copy_infos);

}
