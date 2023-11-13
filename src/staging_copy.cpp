//
// Created by crocdialer on 10.01.23.
//

#define VK_NO_PROTOTYPES
#include <volk.h>
#include <set>
#include "vierkant/staging_copy.hpp"

namespace vierkant
{

size_t staging_copy(staging_copy_context_t &context, const std::vector<staging_copy_info_t> &staging_copy_infos)
{
    assert(context.command_buffer && context.staging_buffer);

    // resize staging-buffer if necessary
    size_t num_staging_bytes = context.offset;
    for(const auto &info: staging_copy_infos) { num_staging_bytes += info.num_bytes; }
    num_staging_bytes = std::max<size_t>(num_staging_bytes, 1UL << 20);
    context.staging_buffer->set_data(nullptr, num_staging_bytes);

    std::vector<VkBufferMemoryBarrier2> barriers;
    std::map<vierkant::Buffer*, std::vector<VkBufferCopy2>> buffer_copies;

    for(const auto &info: staging_copy_infos)
    {
        if(!info.data || !info.num_bytes) { continue; }
        assert(info.dst_buffer);
        assert(context.staging_buffer->num_bytes() - info.num_bytes >= context.offset);

        // copy array into staging-buffer
        auto staging_data = static_cast<uint8_t *>(context.staging_buffer->map()) + context.offset;
        memcpy(staging_data, info.data, info.num_bytes);

        // resize if necessary
        info.dst_buffer->set_data(nullptr, info.num_bytes);

        VkBufferCopy2 copy_region = {};
        copy_region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        copy_region.size = info.num_bytes;
        copy_region.srcOffset = context.offset;
        copy_region.dstOffset = info.dst_offset;
        buffer_copies[info.dst_buffer.get()].push_back(copy_region);

        context.offset += info.num_bytes;

        if(info.dst_stage && info.dst_access && !buffer_copies.contains(info.dst_buffer.get()))
        {
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

    for(const auto &[buf, copies] : buffer_copies)
    {
        VkCopyBufferInfo2 copy_info2 = {};
        copy_info2.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        copy_info2.srcBuffer = context.staging_buffer->handle();
        copy_info2.dstBuffer = buf->handle();
        copy_info2.regionCount = copies.size();
        copy_info2.pRegions = copies.data();
        vkCmdCopyBuffer2(context.command_buffer, &copy_info2);
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

}// namespace vierkant