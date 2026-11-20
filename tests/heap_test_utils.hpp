// helpers for tests comparing descriptor-sets against a descriptor-heap

#pragma once

#include "test_context.hpp"
#include <vierkant/PipelineCache.hpp>

namespace heap_test
{

inline bool descriptor_heap_supported()
{
    vierkant::Instance instance(vierkant::Instance::create_info_t{});
    return !instance.physical_devices().empty() &&
           vierkant::check_device_extension_support(instance.physical_devices()[0],
                                                    vierkant::DescriptorHeap::required_extensions());
}

//! runs fn(device, pipeline_cache) for descriptor-sets [0] and descriptor-heap [1], each on its own device.
//! a fresh device per path keeps one path's output out of memory the other path reads back.
template<typename Fn>
auto per_path(Fn fn, std::vector<const char *> extensions = {})
{
    for(const auto &ext: vierkant::DescriptorHeap::required_extensions()) { extensions.push_back(ext); }

    std::array<decltype(fn(vierkant::DevicePtr{}, vierkant::PipelineCachePtr{})), 2> ret;
    for(uint32_t path = 0; path < 2; ++path)
    {
        vulkan_test_context_t test_context(extensions);
        const auto &device = test_context.device;
        auto cache = path ? vierkant::PipelineCache::create(device, vierkant::DescriptorHeap::create({device}))
                          : vierkant::PipelineCache::create(device);
        ret[path] = fn(device, cache);
    }
    return ret;
}

inline std::vector<uint8_t> read_image(const vierkant::DevicePtr &device, const vierkant::ImagePtr &img, uint32_t level)
{
    VkExtent3D extent = {std::max(1u, img->width() >> level), std::max(1u, img->height() >> level),
                         std::max(1u, img->depth() >> level)};
    size_t num_bytes = vierkant::num_bytes(img->format().format) * extent.width * extent.height * extent.depth;
    auto host_buf = vierkant::Buffer::create(device, nullptr, num_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VMA_MEMORY_USAGE_CPU_ONLY);
    vierkant::CommandBuffer cmd(device, device->command_pool_transient());
    cmd.begin();
    img->copy_to(host_buf, cmd.handle(), 0, {0, 0, 0}, extent, 0, level);
    cmd.submit(device->queue(), true);
    const auto *data = static_cast<const uint8_t *>(host_buf->map());
    return {data, data + num_bytes};
}

//! number of differing bytes and the largest byte-difference, for failure messages
inline std::string diff_stats(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
{
    if(a.size() != b.size()) { return "size " + std::to_string(a.size()) + " vs " + std::to_string(b.size()); }
    size_t num_diff = 0;
    int max_diff = 0;
    for(size_t i = 0; i < a.size(); ++i)
    {
        num_diff += a[i] != b[i];
        max_diff = std::max(max_diff, std::abs(int(a[i]) - int(b[i])));
    }
    return std::to_string(num_diff) + " of " + std::to_string(a.size()) + " bytes differ, max " +
           std::to_string(max_diff);
}

inline bool all_zero(const std::vector<uint8_t> &bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b == 0; });
}

}// namespace heap_test
