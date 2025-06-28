#include "vierkant/staging_copy.hpp"
#include <set>

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

    struct copy_asset_t
    {
        std::vector<VkBufferCopy2> copies;
        VkBufferMemoryBarrier2 barrier{};
    };
    std::map<vierkant::Buffer *, copy_asset_t> copy_assets;

    for(const auto &info: staging_copy_infos)
    {
        if(!info.data || !info.num_bytes || !info.dst_buffer) { continue; }
        assert(context.staging_buffer->num_bytes() - info.num_bytes >= context.offset);

        // copy array into staging-buffer
        auto staging_data = static_cast<uint8_t *>(context.staging_buffer->map()) + context.offset;
        memcpy(staging_data, info.data, info.num_bytes);

        VkBufferCopy2 copy_region = {};
        copy_region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        copy_region.size = info.num_bytes;
        copy_region.srcOffset = context.offset;
        copy_region.dstOffset = info.dst_offset;

        auto &copy_asset = copy_assets[info.dst_buffer.get()];
        copy_asset.copies.push_back(copy_region);

        context.offset += info.num_bytes;

        if(info.dst_stage && info.dst_access)
        {
            VkBufferMemoryBarrier2 &barrier = copy_asset.barrier;
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.buffer = info.dst_buffer->handle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = info.dst_stage;
            barrier.dstAccessMask = info.dst_access;
        }
    }

    std::vector<VkBufferMemoryBarrier2> barriers;
    barriers.reserve(copy_assets.size());

    for(auto &[buf, copy_asset]: copy_assets)
    {
        VkDeviceSize num_bytes = 0;
        for(const auto &copy: copy_asset.copies) { num_bytes = std::max(num_bytes, copy.size + copy.dstOffset); }

        // resize if necessary
        buf->set_data(nullptr, num_bytes);

        VkCopyBufferInfo2 copy_info2 = {};
        copy_info2.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        copy_info2.srcBuffer = context.staging_buffer->handle();
        copy_info2.dstBuffer = buf->handle();
        copy_info2.regionCount = copy_asset.copies.size();
        copy_info2.pRegions = copy_asset.copies.data();
        vkCmdCopyBuffer2(context.command_buffer, &copy_info2);

        // potentially correct handle here (might have changed after growing), push barrier
        copy_asset.barrier.buffer = buf->handle();
        barriers.push_back(copy_asset.barrier);
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