#include <deque>
#include <vierkant/heap_descriptors.hpp>

namespace vierkant
{

namespace
{

uint32_t align_up(uint32_t value, uint32_t alignment) { return (value + alignment - 1) / alignment * alignment; }

VkSpirvResourceTypeFlagsEXT resource_mask(VkDescriptorType type)
{
    switch(type)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT |
                   VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT | VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            return VK_SPIRV_RESOURCE_TYPE_ACCELERATION_STRUCTURE_BIT_EXT;

        // inline uniform-blocks are uniform-buffer data inside the resource-heap
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK: return VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
        default: break;
    }
    throw std::runtime_error("descriptor-heap: unsupported descriptor-type -> " + std::to_string(type));
}

bool is_image(VkDescriptorType type)
{
    return type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
           type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

}// namespace

///////////////////////////////////////////////////////////////////////////////////////////////////

heap_block_layout_t create_heap_block_layout(const vierkant::DevicePtr &device, const descriptor_map_t &descriptors)
{
    const auto &props = device->properties().descriptor_heap;

    heap_block_layout_t ret;
    ret.alignment = std::max(props.imageDescriptorAlignment, props.bufferDescriptorAlignment);
    ret.sampler_alignment = props.samplerDescriptorAlignment;

    for(const auto &[binding, desc]: descriptors)
    {
        if(!ret.bindings.empty() && ret.bindings.back().variable_count)
        {
            throw std::runtime_error("descriptor-heap: variable-count binding must be the last binding");
        }

        // throws for unsupported types
        resource_mask(desc.type);

        heap_block_layout_t::binding_t b = {};
        b.binding = binding;
        b.type = desc.type;
        b.variable_count = desc.variable_count;
        b.count = std::max<uint32_t>({1, static_cast<uint32_t>(desc.images.size()),
                                      static_cast<uint32_t>(desc.buffers.size()),
                                      static_cast<uint32_t>(desc.acceleration_structures.size())});

        uint32_t size = is_image(desc.type) ? props.imageDescriptorSize : props.bufferDescriptorSize;
        uint32_t alignment = is_image(desc.type) ? props.imageDescriptorAlignment : props.bufferDescriptorAlignment;

        if(desc.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
            // raw data, read as a uniform-buffer
            b.count = 1;
            size = static_cast<uint32_t>(desc.inline_uniform_block.size());
            alignment = static_cast<uint32_t>(device->properties().core.limits.minUniformBufferOffsetAlignment);
            ret.alignment = std::max(ret.alignment, alignment);
        }
        b.offset = align_up(ret.num_bytes, alignment);
        b.stride = align_up(size, alignment);
        if(!b.variable_count) { ret.num_bytes = b.offset + b.count * b.stride; }

        if(desc.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            b.sampler_offset = align_up(ret.num_sampler_bytes, props.samplerDescriptorAlignment);
            b.sampler_stride = align_up(props.samplerDescriptorSize, props.samplerDescriptorAlignment);
            if(!b.variable_count) { ret.num_sampler_bytes = b.sampler_offset + b.count * b.sampler_stride; }
        }
        ret.bindings.push_back(b);
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

HeapBlockLayoutPtr find_or_create_heap_block_layout(const vierkant::DevicePtr &device, descriptor_map_t descriptors,
                                                    std::unordered_map<descriptor_map_t, HeapBlockLayoutPtr> &cache)
{
    // keep types and array-lengths, drop everything else
    for(auto &[binding, desc]: descriptors)
    {
        if(desc.variable_count)
        {
            desc.images.clear();
            desc.buffers.clear();
            desc.acceleration_structures.clear();
        }
        for(auto &img: desc.images) { img.reset(); }
        for(auto &buf: desc.buffers) { buf.reset(); }
        for(auto &as: desc.acceleration_structures) { as.reset(); }
        desc.image_views.clear();
        desc.buffer_ranges.clear();
        std::fill(desc.inline_uniform_block.begin(), desc.inline_uniform_block.end(), 0);
    }

    auto it = cache.find(descriptors);
    if(it == cache.end())
    {
        auto layout = std::make_shared<heap_block_layout_t>(create_heap_block_layout(device, descriptors));
        it = cache.emplace(std::move(descriptors), std::move(layout)).first;
    }
    return it->second;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

heap_block_size_t heap_block_size(const heap_block_layout_t &layout, const descriptor_map_t &descriptors)
{
    heap_block_size_t ret = {layout.num_bytes, layout.num_sampler_bytes};

    for(const auto &b: layout.bindings)
    {
        if(!b.variable_count) { continue; }
        const auto &desc = descriptors.at(b.binding);
        auto count = static_cast<uint32_t>(std::max({desc.images.size(), desc.buffers.size(), size_t(1)}));
        ret.num_bytes = std::max(ret.num_bytes, b.offset + count * b.stride);
        if(b.sampler_stride)
        {
            ret.num_sampler_bytes = std::max(ret.num_sampler_bytes, b.sampler_offset + count * b.sampler_stride);
        }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t heap_push_offset(const std::vector<VkPushConstantRange> &push_constant_ranges)
{
    uint32_t end = 0;
    for(const auto &range: push_constant_ranges) { end = std::max(end, range.offset + range.size); }
    return align_up(end, 4);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_push_heap_data(VkCommandBuffer command_buffer, const std::vector<VkPushConstantRange> &push_constant_ranges,
                        std::span<const uint8_t> push_constants, std::span<const heap_block_t> blocks)
{
    uint32_t push_offset = heap_push_offset(push_constant_ranges);
    if(push_constants.size() > push_offset)
    {
        throw std::runtime_error("descriptor-heap: push-constants exceed the pipeline's push-constant ranges");
    }
    std::vector<uint8_t> push_data(push_offset + blocks.size_bytes());
    memcpy(push_data.data(), push_constants.data(), push_constants.size());
    memcpy(push_data.data() + push_offset, blocks.data(), blocks.size_bytes());

    VkPushDataInfoEXT push_info = {};
    push_info.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT;
    push_info.data = {push_data.data(), push_data.size()};
    vkCmdPushDataEXT(command_buffer, &push_info);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<VkDescriptorSetAndBindingMappingEXT> create_heap_mappings(const vierkant::DevicePtr &device,
                                                                      const heap_block_layout_t &layout, uint32_t set,
                                                                      uint32_t push_offset)
{
    if(push_offset + sizeof(heap_block_t) > device->properties().descriptor_heap.maxPushDataSize)
    {
        throw std::runtime_error("descriptor-heap: push-data exceeds maxPushDataSize");
    }

    std::vector<VkDescriptorSetAndBindingMappingEXT> ret;

    for(const auto &b: layout.bindings)
    {
        VkDescriptorSetAndBindingMappingEXT mapping = {};
        mapping.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        mapping.descriptorSet = set;
        mapping.firstBinding = b.binding;
        mapping.bindingCount = 1;
        mapping.resourceMask = resource_mask(b.type);

        // the pushed resource-offset is a byte-offset, used as-is by both sources
        const uint32_t resource_push_offset = push_offset + offsetof(heap_block_t, resource_offset);

        if(b.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
            mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT;
            mapping.sourceData.heapData.heapOffset = b.offset;
            mapping.sourceData.heapData.pushOffset = resource_push_offset;
            ret.push_back(mapping);
            continue;
        }
        mapping.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;

        auto &push_index = mapping.sourceData.pushIndex;
        push_index.heapOffset = b.offset;
        push_index.pushOffset = resource_push_offset;
        push_index.heapIndexStride = 1;
        push_index.heapArrayStride = b.stride;

        if(b.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            push_index.samplerHeapOffset = b.sampler_offset;
            push_index.samplerPushOffset = push_offset + offsetof(heap_block_t, sampler_index);
            push_index.samplerHeapIndexStride = layout.sampler_alignment;
            push_index.samplerHeapArrayStride = b.sampler_stride;
        }
        ret.push_back(mapping);
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

heap_block_t write_heap_block(const vierkant::DevicePtr &device, const heap_block_layout_t &layout,
                              const descriptor_map_t &descriptors, DescriptorArena &resource_arena,
                              DescriptorArena &sampler_arena)
{
    const auto &props = device->properties().descriptor_heap;
    const auto [num_bytes, num_sampler_bytes] = heap_block_size(layout, descriptors);

    heap_block_t ret = {};
    DescriptorArena::allocation_t block = {}, sampler_block = {};

    if(num_bytes)
    {
        block = resource_arena.allocate(num_bytes, layout.alignment);
        ret.resource_offset = static_cast<uint32_t>(block.offset);
    }
    if(num_sampler_bytes)
    {
        sampler_block = sampler_arena.allocate(num_sampler_bytes, layout.sampler_alignment);
        ret.sampler_index = static_cast<uint32_t>(sampler_block.offset / layout.sampler_alignment);
    }

    // write-inputs, pointers into these must stay valid until the writes
    std::vector<VkResourceDescriptorInfoEXT> resource_infos;
    std::vector<VkHostAddressRangeEXT> resource_dst;
    std::vector<VkSamplerCreateInfo> sampler_infos;
    std::vector<VkHostAddressRangeEXT> sampler_dst;
    std::deque<VkDeviceAddressRangeEXT> address_ranges;
    std::deque<VkImageDescriptorInfoEXT> image_infos;
    std::deque<VkSamplerReductionModeCreateInfo> reduction_infos;

    auto add_resource = [&](VkDescriptorType type, VkResourceDescriptorDataEXT data, uint32_t offset) {
        VkResourceDescriptorInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
        info.type = type;
        info.data = data;
        resource_infos.push_back(info);
        const uint32_t size = is_image(type) ? props.imageDescriptorSize : props.bufferDescriptorSize;
        resource_dst.push_back({static_cast<uint8_t *>(block.data) + offset, size});
    };

    for(const auto &b: layout.bindings)
    {
        const auto &desc = descriptors.at(b.binding);

        switch(desc.type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                uint32_t j = 0;
                for(uint32_t i = 0; i < desc.buffers.size(); ++i)
                {
                    const auto &buf = desc.buffers[i];
                    if(!buf || !buf->handle()) { continue; }
                    if(!buf->device_address())
                    {
                        throw std::runtime_error("descriptor-heap: buffer without device-address");
                    }
                    auto range = i < desc.buffer_ranges.size() ? desc.buffer_ranges[i] : descriptor_t::buffer_range_t{};
                    auto &address_range = address_ranges.emplace_back();
                    address_range.address = buf->device_address() + range.offset;
                    address_range.size = range.range ? range.range : buf->num_bytes() - range.offset;
                    add_resource(desc.type, {.pAddressRange = &address_range}, b.offset + j++ * b.stride);
                }
                break;
            }

            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            {
                uint32_t j = 0;
                for(uint32_t i = 0; i < desc.images.size(); ++i)
                {
                    const auto &img = desc.images[i];
                    if(!img) { continue; }

                    auto &image_info = image_infos.emplace_back();
                    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT;
                    image_info.pView = i < desc.image_views.size() ? &desc.image_views[i].info : &img->view().info;
                    image_info.layout = img->image_layout();
                    if(image_info.layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
                    {
                        image_info.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                    }

                    // combined image-samplers are sampled images in the resource-heap
                    auto resource_type = desc.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                                 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                                                 : desc.type;
                    add_resource(resource_type, {.pImage = &image_info}, b.offset + j * b.stride);

                    if(desc.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    {
                        sampler_infos.push_back(
                                sampler_create_info(img->sampler_state(), reduction_infos.emplace_back()));
                        sampler_dst.push_back(
                                {static_cast<uint8_t *>(sampler_block.data) + b.sampler_offset + j * b.sampler_stride,
                                 props.samplerDescriptorSize});
                    }
                    j++;
                }
                break;
            }

            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
                memcpy(static_cast<uint8_t *>(block.data) + b.offset, desc.inline_uniform_block.data(),
                       desc.inline_uniform_block.size());
                break;

            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            {
                for(uint32_t i = 0; i < desc.acceleration_structures.size(); ++i)
                {
                    VkAccelerationStructureDeviceAddressInfoKHR address_info = {};
                    address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                    address_info.accelerationStructure = desc.acceleration_structures[i].get();

                    // size 0: the range is not checked against the backing buffer
                    auto &address_range = address_ranges.emplace_back();
                    address_range.address = vkGetAccelerationStructureDeviceAddressKHR(device->handle(), &address_info);
                    add_resource(desc.type, {.pAddressRange = &address_range}, b.offset + i * b.stride);
                }
                break;
            }
            default: resource_mask(desc.type);
        }
    }

    if(!resource_infos.empty())
    {
        vkCheck(vkWriteResourceDescriptorsEXT(device->handle(), resource_infos.size(), resource_infos.data(),
                                              resource_dst.data()),
                "write_heap_block: vkWriteResourceDescriptorsEXT failed");
    }
    if(!sampler_infos.empty())
    {
        vkCheck(vkWriteSamplerDescriptorsEXT(device->handle(), sampler_infos.size(), sampler_infos.data(),
                                             sampler_dst.data()),
                "write_heap_block: vkWriteSamplerDescriptorsEXT failed");
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

HeapBlockAllocator::HeapBlockAllocator(DescriptorHeapPtr heap) : m_heap(std::move(heap)) {}

heap_block_t HeapBlockAllocator::write(const heap_block_layout_t &layout, const descriptor_map_t &descriptors)
{
    auto [num_bytes, num_sampler_bytes] = heap_block_size(layout, descriptors);
    auto &resource_arena = arena(DescriptorHeapType::RESOURCE, num_bytes, layout.alignment);
    auto &sampler_arena = arena(DescriptorHeapType::SAMPLER, num_sampler_bytes, layout.sampler_alignment);
    return write_heap_block(m_heap->device(), layout, descriptors, resource_arena, sampler_arena);
}

void HeapBlockAllocator::reset()
{
    for(auto &arenas: m_arenas)
    {
        if(arenas.size() == 1) { arenas.back()->reset(); }
        else if(arenas.size() > 1)
        {
            // merge into one arena of the combined size
            auto type = arenas.back()->type();
            VkDeviceSize num_bytes = 0;
            for(const auto &a: arenas) { num_bytes += a->num_bytes(); }
            arenas.clear();
            arenas.push_back(m_heap->create_arena(type, num_bytes));
        }
    }
}

DescriptorArena &HeapBlockAllocator::arena(DescriptorHeapType type, VkDeviceSize num_bytes, VkDeviceSize alignment)
{
    // initial arena-sizes, they double when exhausted
    constexpr VkDeviceSize initial_resource_bytes = 16 << 10, initial_sampler_bytes = 1 << 10;

    auto &arenas = m_arenas[static_cast<uint32_t>(type)];
    if(arenas.empty() || !arenas.back()->can_allocate(num_bytes, alignment))
    {
        VkDeviceSize size =
                arenas.empty() ? (type == DescriptorHeapType::RESOURCE ? initial_resource_bytes : initial_sampler_bytes)
                               : 2 * arenas.back()->num_bytes();
        arenas.push_back(m_heap->create_arena(type, std::max(size, num_bytes + alignment)));
    }
    return *arenas.back();
}

}// namespace vierkant
