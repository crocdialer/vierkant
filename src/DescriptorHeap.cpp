//
// Created by crocdialer on 11/19/26.
//

#include <format>
#include <vierkant/DescriptorHeap.hpp>

namespace vierkant
{

namespace
{
constexpr VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment)
{ return (value + alignment - 1) / alignment * alignment; }
}// namespace

std::vector<const char *> DescriptorHeap::required_extensions() { return {VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME}; }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorHeapPtr DescriptorHeap::create(const create_info_t &create_info)
{ return DescriptorHeapPtr(new DescriptorHeap(create_info)); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorHeap::DescriptorHeap(const create_info_t &create_info) : m_device(create_info.device)
{
    if(!m_device->features().descriptor_heap.descriptorHeap)
    {
        throw std::runtime_error("vierkant::DescriptorHeap: device created without descriptorHeap-feature");
    }
    const auto &props = m_device->properties().descriptor_heap;

    struct heap_params_t
    {
        DescriptorHeapType type;
        VkDeviceSize num_bytes, max_num_bytes, heap_alignment, reserved_range, arena_alignment;
        const char *suffix;
    };
    const heap_params_t params[] = {
            {DescriptorHeapType::RESOURCE, create_info.resource_heap_size, props.maxResourceHeapSize,
             props.resourceHeapAlignment, props.minResourceHeapReservedRange,
             std::max(props.imageDescriptorAlignment, props.bufferDescriptorAlignment), "resource"},
            {DescriptorHeapType::SAMPLER, create_info.sampler_heap_size, props.maxSamplerHeapSize,
             props.samplerHeapAlignment, props.minSamplerHeapReservedRange, props.samplerDescriptorAlignment,
             "sampler"}};

    for(const auto &p: params)
    {
        auto &heap = m_heaps[static_cast<uint32_t>(p.type)];
        VkDeviceSize num_bytes = std::min(p.num_bytes, p.max_num_bytes);

        // reserved range first, offset 0 satisfies every descriptor-alignment
        heap.arena_alignment = p.arena_alignment;
        heap.arena_offset = align_up(p.reserved_range, p.arena_alignment);
        if(num_bytes <= heap.arena_offset)
        {
            throw std::runtime_error(std::format("vierkant::DescriptorHeap: {}-heap of {} bytes leaves no space past "
                                                 "a reserved range of {} bytes",
                                                 p.suffix, num_bytes, p.reserved_range));
        }

        vierkant::Buffer::create_info_t buffer_info = {};
        buffer_info.device = m_device;
        buffer_info.num_bytes = num_bytes;
        buffer_info.alignment = static_cast<uint32_t>(p.heap_alignment);
        buffer_info.usage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        buffer_info.name = std::format("{}{}descriptor-heap ({})", create_info.name,
                                       create_info.name.empty() ? "" : " ", p.suffix);
        heap.buffer = vierkant::Buffer::create(buffer_info);
        heap.data = static_cast<uint8_t *>(heap.buffer->map());

        heap.bind_info.sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT;
        heap.bind_info.heapRange = {heap.buffer->device_address(), num_bytes};
        heap.bind_info.reservedRangeOffset = 0;
        heap.bind_info.reservedRangeSize = p.reserved_range;

        VmaVirtualBlockCreateInfo block_info = {};
        block_info.size = num_bytes - heap.arena_offset;
        vkCheck(vmaCreateVirtualBlock(&block_info, &heap.virtual_block), "could not create virtual block");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorHeap::~DescriptorHeap()
{
    // arenas keep the heap alive, so all virtual allocations are gone by now
    for(auto &heap: m_heaps) { vmaDestroyVirtualBlock(heap.virtual_block); }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorArenaPtr DescriptorHeap::create_arena(DescriptorHeapType type, VkDeviceSize num_bytes)
{
    auto &heap = m_heaps[static_cast<uint32_t>(type)];

    VmaVirtualAllocationCreateInfo alloc_info = {};
    alloc_info.size = num_bytes;
    alloc_info.alignment = heap.arena_alignment;

    VmaVirtualAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkResult result;
    {
        std::lock_guard lock(m_mutex);
        result = vmaVirtualAllocate(heap.virtual_block, &alloc_info, &allocation, &offset);
    }
    if(result != VK_SUCCESS)
    {
        throw std::runtime_error(
                std::format("vierkant::DescriptorHeap: could not create arena of {} bytes", num_bytes));
    }
    return DescriptorArenaPtr(
            new DescriptorArena(shared_from_this(), type, heap.arena_offset + offset, num_bytes, allocation));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DescriptorHeap::free_arena(DescriptorHeapType type, VmaVirtualAllocation allocation)
{
    std::lock_guard lock(m_mutex);
    vmaVirtualFree(m_heaps[static_cast<uint32_t>(type)].virtual_block, allocation);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DescriptorHeap::bind(VkCommandBuffer command_buffer) const
{
    vkCmdBindResourceHeapEXT(command_buffer, &bind_info(DescriptorHeapType::RESOURCE));
    vkCmdBindSamplerHeapEXT(command_buffer, &bind_info(DescriptorHeapType::SAMPLER));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const VkBindHeapInfoEXT &DescriptorHeap::bind_info(DescriptorHeapType type) const
{ return m_heaps[static_cast<uint32_t>(type)].bind_info; }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const vierkant::BufferPtr &DescriptorHeap::buffer(DescriptorHeapType type) const
{ return m_heaps[static_cast<uint32_t>(type)].buffer; }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorArena::DescriptorArena(DescriptorHeapPtr heap, DescriptorHeapType type, VkDeviceSize offset,
                                 VkDeviceSize num_bytes, VmaVirtualAllocation allocation)
    : m_heap(std::move(heap)), m_type(type), m_offset(offset), m_num_bytes(num_bytes), m_allocation(allocation)
{}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorArena::~DescriptorArena() { m_heap->free_arena(m_type, m_allocation); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DescriptorArena::can_allocate(VkDeviceSize num_bytes, VkDeviceSize alignment) const
{
    VkDeviceSize offset = align_up(m_offset + m_num_bytes_used, std::max<VkDeviceSize>(alignment, 1));
    return offset + num_bytes <= m_offset + m_num_bytes;
}

DescriptorArena::allocation_t DescriptorArena::allocate(VkDeviceSize num_bytes, VkDeviceSize alignment)
{
    // align relative to the heap-start, which is what descriptor-alignments refer to
    VkDeviceSize offset = align_up(m_offset + m_num_bytes_used, std::max<VkDeviceSize>(alignment, 1));
    if(offset + num_bytes > m_offset + m_num_bytes)
    {
        throw std::runtime_error(std::format("vierkant::DescriptorArena: exhausted, {} of {} bytes in use, "
                                             "requested {}",
                                             m_num_bytes_used, m_num_bytes, num_bytes));
    }
    m_num_bytes_used = offset + num_bytes - m_offset;
    auto *heap_data = m_heap->m_heaps[static_cast<uint32_t>(m_type)].data;
    return {offset, heap_data + offset, num_bytes};
}

}// namespace vierkant
