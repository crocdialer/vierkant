#include "test_context.hpp"
#include <vierkant/DescriptorHeap.hpp>
#include <vierkant/Image.hpp>

namespace
{
bool descriptor_heap_supported()
{
    vierkant::Instance instance(vierkant::Instance::create_info_t{});
    return !instance.physical_devices().empty() &&
           vierkant::check_device_extension_support(instance.physical_devices()[0],
                                                    vierkant::DescriptorHeap::required_extensions());
}

VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment)
{ return (value + alignment - 1) / alignment * alignment; }

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

TEST(DescriptorHeap, Arenas)
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

    // arenas sit past the reserved range and do not overlap
    auto a = heap->create_arena(vierkant::DescriptorHeapType::RESOURCE, 4096);
    auto b = heap->create_arena(vierkant::DescriptorHeapType::RESOURCE, 4096);
    for(const auto &arena: {a, b})
    {
        EXPECT_GE(arena->offset(), resource_bind.reservedRangeOffset + resource_bind.reservedRangeSize);
        EXPECT_LE(arena->offset() + arena->num_bytes(), resource_bind.heapRange.size);
    }
    EXPECT_TRUE(a->offset() + a->num_bytes() <= b->offset() || b->offset() + b->num_bytes() <= a->offset());

    // allocations are aligned relative to the heap-start
    auto first = a->allocate(1, 1);
    EXPECT_EQ(first.offset, a->offset());
    auto aligned = a->allocate(16, 256);
    EXPECT_EQ(aligned.offset % 256, 0);
    EXPECT_GT(aligned.offset, first.offset);
    EXPECT_EQ(static_cast<uint8_t *>(aligned.data) - static_cast<uint8_t *>(first.data), aligned.offset - first.offset);

    // exhaustion throws, reset releases everything
    EXPECT_THROW(a->allocate(a->num_bytes(), 1), std::runtime_error);
    a->reset();
    EXPECT_EQ(a->allocate(a->num_bytes(), 1).offset, a->offset());

    // a destroyed arena returns its range to the heap
    VkDeviceSize sampler_space =
            sampler_bind.heapRange.size - align_up(sampler_bind.reservedRangeSize, props.samplerDescriptorAlignment);
    auto all_samplers = heap->create_arena(vierkant::DescriptorHeapType::SAMPLER, sampler_space);
    EXPECT_THROW(heap->create_arena(vierkant::DescriptorHeapType::SAMPLER, props.samplerDescriptorSize),
                 std::runtime_error);
    all_samplers.reset();
    EXPECT_NO_THROW(heap->create_arena(vierkant::DescriptorHeapType::SAMPLER, sampler_space));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(DescriptorHeap, WriteAndBind)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &device = test_context.device;
    const auto &props = device->properties().descriptor_heap;
    auto heap = vierkant::DescriptorHeap::create({device});
    auto resource_arena = heap->create_arena(vierkant::DescriptorHeapType::RESOURCE, 4096);
    auto sampler_arena = heap->create_arena(vierkant::DescriptorHeapType::SAMPLER, 1024);

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

    auto buffer_alloc = resource_arena->allocate(props.bufferDescriptorSize, props.bufferDescriptorAlignment);
    auto image_alloc = resource_arena->allocate(props.imageDescriptorSize, props.imageDescriptorAlignment);
    std::array<VkHostAddressRangeEXT, 2> resource_dst = {
            VkHostAddressRangeEXT{buffer_alloc.data, static_cast<size_t>(buffer_alloc.num_bytes)},
            VkHostAddressRangeEXT{image_alloc.data, static_cast<size_t>(image_alloc.num_bytes)}};

    // sampler descriptor
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;

    auto sampler_alloc = sampler_arena->allocate(props.samplerDescriptorSize, props.samplerDescriptorAlignment);
    VkHostAddressRangeEXT sampler_dst = {sampler_alloc.data, static_cast<size_t>(sampler_alloc.num_bytes)};

    // the writes land in the arena-allocations
    for(const auto &alloc: {buffer_alloc, image_alloc, sampler_alloc}) { memset(alloc.data, 0, alloc.num_bytes); }
    vierkant::vkCheck(vkWriteResourceDescriptorsEXT(device->handle(), resource_infos.size(), resource_infos.data(),
                                                    resource_dst.data()),
                      "vkWriteResourceDescriptorsEXT failed");
    vierkant::vkCheck(vkWriteSamplerDescriptorsEXT(device->handle(), 1, &sampler_info, &sampler_dst),
                      "vkWriteSamplerDescriptorsEXT failed");
    EXPECT_FALSE(all_zero(buffer_alloc.data, buffer_alloc.num_bytes));
    EXPECT_FALSE(all_zero(image_alloc.data, image_alloc.num_bytes));

    // bind both heaps and submit
    vierkant::CommandBuffer cmd(device, device->command_pool_transient());
    cmd.begin();
    heap->bind(cmd.handle());
    cmd.end();
    cmd.submit(device->queue(), true);
}
