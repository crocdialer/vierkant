//
// Created by crocdialer on 10.01.23.
//

#include <set>
#include "vierkant/staging_copy.hpp"

namespace vierkant
{

size_t staging_copy(staging_copy_context_t &context,
                    const std::vector<staging_copy_info_t> &staging_copy_infos)
{
    assert(context.command_buffer && context.staging_buffer);

    // resize staging-buffer if necessary
    size_t num_staging_bytes = context.offset;
    for(const auto &info: staging_copy_infos){ num_staging_bytes += info.num_bytes; }
    num_staging_bytes = std::max<size_t>(num_staging_bytes, 1UL << 20);
    context.staging_buffer->set_data(nullptr, num_staging_bytes);

    std::vector<VkBufferMemoryBarrier2> barriers;
    std::set<vierkant::Buffer *> buffer_set;

    for(const auto &info: staging_copy_infos)
    {
        assert(context.staging_buffer->num_bytes() - info.num_bytes >= context.offset);

        // copy array into staging-buffer
        auto staging_data = static_cast<uint8_t *>(context.staging_buffer->map()) + context.offset;
        memcpy(staging_data, info.data, info.num_bytes);

        // resize if necessary
        info.dst_buffer->set_data(nullptr, info.num_bytes);

        // issue copy from staging-buffer to GPU-buffer
        context.staging_buffer->copy_to(info.dst_buffer, context.command_buffer, context.offset, info.dst_offset,
                                        info.num_bytes);
        context.offset += info.num_bytes;

        if(info.dst_stage && info.dst_access && !buffer_set.contains(info.dst_buffer.get()))
        {
            buffer_set.insert(info.dst_buffer.get());

            VkBufferMemoryBarrier2 barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.buffer = info.dst_buffer->handle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = info.dst_stage;
            barrier.dstAccessMask = info.dst_access;
            barriers.push_back(barrier);
        }
    }

    if(!barriers.empty())
    {
        VkDependencyInfo dependency_info = {};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.bufferMemoryBarrierCount = barriers.size();
        dependency_info.pBufferMemoryBarriers = barriers.data();
        vkCmdPipelineBarrier2(context.command_buffer, &dependency_info);
    }
    return context.offset;
}

}