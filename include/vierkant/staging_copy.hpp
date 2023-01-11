//
// Created by crocdialer on 10.01.23.
//

#pragma once

#include <vierkant/Buffer.hpp>

namespace vierkant
{

struct staging_copy_info_t
{
    const void *data = nullptr;
    size_t num_bytes = 0;
    vierkant::BufferPtr dst_buffer;
    VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 dst_access = VK_ACCESS_2_NONE;
};

/**
 * @brief   staging_copy can be used to schedule a list of staging-copies,
 *          using provided commandbuffer and staging-buffer.
 *
 * @param   command_buffer      a provided command-buffer.
 * @param   staging_buffer      a provided host-visible buffer.
 * @param   staging_copy_infos  an array of copy-infos
 * @return  total number of bytes (size of staging-buffer)
 */
size_t staging_copy(VkCommandBuffer command_buffer,
                    const vierkant::BufferPtr& staging_buffer,
                    const std::vector<staging_copy_info_t> &staging_copy_infos);

}
