//
// Created by crocdialer on 11/19/26.
//

#include <deque>
#include <format>
#include <vierkant/DescriptorHeap.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

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
        VkDeviceSize num_bytes, max_num_bytes, heap_alignment, reserved_range, range_alignment;
        const char *suffix;
    };
    const heap_params_t params[] = {
            {.type = DescriptorHeapType::RESOURCE,
             .num_bytes = create_info.resource_heap_size,
             .max_num_bytes = props.maxResourceHeapSize,
             .heap_alignment = props.resourceHeapAlignment,
             .reserved_range = props.minResourceHeapReservedRange,
             .range_alignment = std::max(props.imageDescriptorAlignment, props.bufferDescriptorAlignment),
             .suffix = "resource"},
            {.type = DescriptorHeapType::SAMPLER,
             .num_bytes = create_info.sampler_heap_size,
             .max_num_bytes = props.maxSamplerHeapSize,
             .heap_alignment = props.samplerHeapAlignment,
             .reserved_range = props.minSamplerHeapReservedRange,
             .range_alignment = props.samplerDescriptorAlignment,
             .suffix = "sampler"}};

    for(const auto &p: params)
    {
        auto &heap = m_heaps[static_cast<uint32_t>(p.type)];
        VkDeviceSize num_bytes = std::min(p.num_bytes, p.max_num_bytes);

        // reserved range first, offset 0 satisfies every descriptor-alignment
        heap.range_alignment = p.range_alignment;
        heap.range_offset = align_up(p.reserved_range, p.range_alignment);
        if(num_bytes <= heap.range_offset)
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
        block_info.size = num_bytes - heap.range_offset;
        vkCheck(vmaCreateVirtualBlock(&block_info, &heap.virtual_block), "could not create virtual block");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorHeap::~DescriptorHeap()
{
    // ranges keep the heap alive, so all virtual allocations are gone by now
    for(const auto &heap: m_heaps) { vmaDestroyVirtualBlock(heap.virtual_block); }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorRangePtr DescriptorHeap::create_range(DescriptorHeapType type, VkDeviceSize num_bytes)
{
    const auto &heap = m_heaps[static_cast<uint32_t>(type)];

    VmaVirtualAllocationCreateInfo alloc_info = {};
    alloc_info.size = num_bytes;
    alloc_info.alignment = heap.range_alignment;

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
                std::format("vierkant::DescriptorHeap: could not create range of {} bytes", num_bytes));
    }
    offset += heap.range_offset;
    return {new DescriptorRange(shared_from_this(), type, offset, num_bytes, heap.data + offset, allocation),
            [](DescriptorRange *range) {
                range->m_heap->free_range(range->type(), range->m_allocation);
                std::default_delete<DescriptorRange>()(range);
            }};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorRangeConstPtr DescriptorHeap::sampler_block(const std::vector<sampler_slot_t> &slots,
                                                      const uint32_t num_bytes)
{
    sampler_block_key_t key = {.num_bytes = num_bytes, .slots = slots};
    std::lock_guard lock(m_sampler_mutex);

    auto &entry = m_sampler_blocks[key];
    if(auto block = entry.lock()) { return block; }

    const auto &props = m_device->properties().descriptor_heap;
    auto range = create_range(DescriptorHeapType::SAMPLER, num_bytes);

    std::vector<VkSamplerCreateInfo> sampler_infos;
    std::vector<VkHostAddressRangeEXT> sampler_dst;
    std::deque<VkSamplerReductionModeCreateInfo> reduction_infos;
    for(const auto &[offset, state]: slots)
    {
        sampler_infos.push_back(sampler_create_info(state, reduction_infos.emplace_back()));
        sampler_dst.push_back({.address = range->data() + offset, .size = props.samplerDescriptorSize});
    }
    if(!sampler_infos.empty())
    {
        vkCheck(vkWriteSamplerDescriptorsEXT(m_device->handle(), sampler_infos.size(), sampler_infos.data(),
                                             sampler_dst.data()),
                "DescriptorHeap::sampler_block: vkWriteSamplerDescriptorsEXT failed");
    }

    // the last holder removes the expired entry, then frees the range
    auto *range_ptr = range.get();
    DescriptorRangeConstPtr block(range_ptr, [this, key = std::move(key), range](const DescriptorRange *) mutable {
        {
            std::lock_guard scope_lock(m_sampler_mutex);
            if(const auto it = m_sampler_blocks.find(key); it != m_sampler_blocks.end() && it->second.expired())
            {
                m_sampler_blocks.erase(it);
            }
        }
        range.reset();
    });
    entry = block;
    return block;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t DescriptorHeap::sampler_block_key_hash_t::operator()(const sampler_block_key_t &key) const
{
    size_t h = 0;
    hash_combine(h, key.num_bytes);
    for(const auto &[offset, state]: key.slots)
    {
        hash_combine(h, offset);
        hash_combine(h, state);
    }
    return h;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DescriptorHeap::free_range(DescriptorHeapType type, VmaVirtualAllocation allocation)
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

DescriptorRange::DescriptorRange(DescriptorHeapPtr heap, DescriptorHeapType type, VkDeviceSize offset,
                                 VkDeviceSize num_bytes, uint8_t *data, VmaVirtualAllocation allocation)
    : m_heap(std::move(heap)), m_type(type), m_offset(offset), m_num_bytes(num_bytes), m_data(data),
      m_allocation(allocation)
{}

}// namespace vierkant
