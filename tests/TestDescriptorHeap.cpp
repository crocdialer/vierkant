#include "heap_test_utils.hpp"
#include <vierkant/DescriptorHeap.hpp>
#include <vierkant/Image.hpp>

using heap_test::descriptor_heap_supported;

namespace
{
bool all_zero(const void *data, size_t num_bytes)
{
    auto *ptr = static_cast<const uint8_t *>(data);
    return std::all_of(ptr, ptr + num_bytes, [](uint8_t b) { return b == 0; });
}
}// namespace

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(DescriptorHeap, Device)
{
    // without the extension: feature off, heap-creation refused
    {
        vulkan_test_context_t test_context;
        EXPECT_FALSE(test_context.device->features().descriptor_heap.descriptorHeap);
        EXPECT_THROW(vierkant::DescriptorHeap::create({test_context.device}), std::runtime_error);
    }
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    EXPECT_TRUE(test_context.device->features().descriptor_heap.descriptorHeap);

    const auto &props = test_context.device->properties().descriptor_heap;
    EXPECT_GT(props.maxResourceHeapSize, 0);
    EXPECT_GT(props.maxSamplerHeapSize, 0);
    EXPECT_GT(props.imageDescriptorSize, 0);
    EXPECT_GT(props.bufferDescriptorSize, 0);
    EXPECT_GT(props.samplerDescriptorSize, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(DescriptorHeap, Ranges)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &props = test_context.device->properties().descriptor_heap;
    auto heap = vierkant::DescriptorHeap::create({test_context.device});

    // heap-ranges and reserved ranges honor device-limits
    const auto &resource_bind = heap->bind_info(vierkant::DescriptorHeapType::RESOURCE);
    const auto &sampler_bind = heap->bind_info(vierkant::DescriptorHeapType::SAMPLER);
    EXPECT_LE(resource_bind.heapRange.size, props.maxResourceHeapSize);
    EXPECT_LE(sampler_bind.heapRange.size, props.maxSamplerHeapSize);
    EXPECT_GE(resource_bind.reservedRangeSize, props.minResourceHeapReservedRange);
    EXPECT_GE(sampler_bind.reservedRangeSize, props.minSamplerHeapReservedRange);
    EXPECT_EQ(resource_bind.heapRange.address % props.resourceHeapAlignment, 0);
    EXPECT_EQ(sampler_bind.heapRange.address % props.samplerHeapAlignment, 0);

    // ranges sit past the reserved range, are aligned and do not overlap
    const auto alignment = std::max(props.imageDescriptorAlignment, props.bufferDescriptorAlignment);
    const auto *heap_data = static_cast<uint8_t *>(heap->buffer(vierkant::DescriptorHeapType::RESOURCE)->map());
    auto a = heap->create_range(vierkant::DescriptorHeapType::RESOURCE, 4096);
    auto b = heap->create_range(vierkant::DescriptorHeapType::RESOURCE, 4096);
    for(const auto &range: {a, b})
    {
        EXPECT_GE(range->offset(), resource_bind.reservedRangeOffset + resource_bind.reservedRangeSize);
        EXPECT_LE(range->offset() + range->num_bytes(), resource_bind.heapRange.size);
        EXPECT_EQ(range->offset() % alignment, 0);
        EXPECT_EQ(range->data(), heap_data + range->offset());
    }
    EXPECT_TRUE(a->offset() + a->num_bytes() <= b->offset() || b->offset() + b->num_bytes() <= a->offset());

    // a full heap throws, a destroyed range returns to the heap
    VkDeviceSize sampler_space = sampler_bind.heapRange.size -
                                 vierkant::align_up(sampler_bind.reservedRangeSize, props.samplerDescriptorAlignment);
    auto all_samplers = heap->create_range(vierkant::DescriptorHeapType::SAMPLER, sampler_space);
    EXPECT_THROW(heap->create_range(vierkant::DescriptorHeapType::SAMPLER, props.samplerDescriptorSize),
                 std::runtime_error);
    all_samplers.reset();
    EXPECT_NO_THROW(heap->create_range(vierkant::DescriptorHeapType::SAMPLER, sampler_space));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(DescriptorHeap, WriteAndBind)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &device = test_context.device;
    const auto &props = device->properties().descriptor_heap;
    auto heap = vierkant::DescriptorHeap::create({device});
    auto resource_range = heap->create_range(vierkant::DescriptorHeapType::RESOURCE, 4096);
    auto sampler_range = heap->create_range(vierkant::DescriptorHeapType::SAMPLER, 1024);

    // storage-buffer descriptor
    constexpr VkDeviceSize num_buffer_bytes = 256;
    auto buffer = vierkant::Buffer::create(
            device, nullptr, num_buffer_bytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    VkDeviceAddressRangeEXT address_range = {buffer->device_address(), num_buffer_bytes};

    // sampled-image descriptor
    vierkant::Image::Format fmt = {};
    fmt.extent = {4, 4, 1};
    fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    auto image = vierkant::Image::create(device, fmt);

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image->image();
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = fmt.format;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageDescriptorInfoEXT image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT;
    image_info.pView = &view_info;
    image_info.layout = image->image_layout();

    std::array<VkResourceDescriptorInfoEXT, 2> resource_infos = {};
    resource_infos[0].sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
    resource_infos[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resource_infos[0].data.pAddressRange = &address_range;
    resource_infos[1].sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
    resource_infos[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    resource_infos[1].data.pImage = &image_info;

    // buffer-descriptor at the range-start, image-descriptor after it
    auto image_offset = vierkant::align_up(props.bufferDescriptorSize, props.imageDescriptorAlignment);
    std::array<VkHostAddressRangeEXT, 2> resource_dst = {
            VkHostAddressRangeEXT{resource_range->data(), static_cast<size_t>(props.bufferDescriptorSize)},
            VkHostAddressRangeEXT{resource_range->data() + image_offset,
                                  static_cast<size_t>(props.imageDescriptorSize)}};

    // sampler descriptor
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;

    VkHostAddressRangeEXT sampler_dst = {sampler_range->data(), static_cast<size_t>(props.samplerDescriptorSize)};

    // the writes land in the ranges
    for(const auto &dst: {resource_dst[0], resource_dst[1], sampler_dst}) { memset(dst.address, 0, dst.size); }
    vierkant::vkCheck(vkWriteResourceDescriptorsEXT(device->handle(), resource_infos.size(), resource_infos.data(),
                                                    resource_dst.data()),
                      "vkWriteResourceDescriptorsEXT failed");
    vierkant::vkCheck(vkWriteSamplerDescriptorsEXT(device->handle(), 1, &sampler_info, &sampler_dst),
                      "vkWriteSamplerDescriptorsEXT failed");
    EXPECT_FALSE(all_zero(resource_dst[0].address, resource_dst[0].size));
    EXPECT_FALSE(all_zero(resource_dst[1].address, resource_dst[1].size));

    // bind both heaps and submit
    vierkant::CommandBuffer cmd(device, device->command_pool_transient());
    cmd.begin();
    heap->bind(cmd.handle());
    cmd.end();
    cmd.submit(device->queue(), true);
}
