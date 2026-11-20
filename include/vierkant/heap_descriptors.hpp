//
// Created by crocdialer on 11/20/26.
//

#pragma once

#include <vierkant/DescriptorHeap.hpp>
#include <vierkant/descriptor.hpp>
#include <vierkant/pipeline_formats.hpp>

namespace vierkant
{

/**
 * @brief   heap_block_layout_t is the layout of one descriptor_map_t in the resource- and sampler-heap.
 *          a block is addressed by an index in push-data (VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT).
 */
struct heap_block_layout_t
{
    struct binding_t
    {
        uint32_t binding = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;

        //! number of descriptors. a variable-count binding is sized when the block is written.
        uint32_t count = 1;
        bool variable_count = false;

        //! byte-offset and array-stride in the resource-block
        uint32_t offset = 0;
        uint32_t stride = 0;

        //! byte-offset and array-stride in the sampler-block, combined image-samplers only
        uint32_t sampler_offset = 0;
        uint32_t sampler_stride = 0;
    };
    std::vector<binding_t> bindings;

    //! block-sizes in bytes, without a variable-count binding
    uint32_t num_bytes = 0;
    uint32_t num_sampler_bytes = 0;

    //! block-alignments, also the index-strides
    uint32_t alignment = 1;
    uint32_t sampler_alignment = 1;
};

//! block-indices in push-data, one per descriptor-set
struct heap_block_t
{
    uint32_t resource_index = 0;
    uint32_t sampler_index = 0;
};

//! block-sizes in bytes for one descriptor-map
struct heap_block_size_t
{
    uint32_t num_bytes = 0;
    uint32_t num_sampler_bytes = 0;
};

/**
 * @brief   create_heap_block_layout derives a block-layout from a descriptor-map.
 *          a variable-count binding must be the last binding.
 *
 * @param   device      handle for a vierkant::Device
 * @param   descriptors a provided descriptor-map
 * @return  the block-layout for @p descriptors
 */
heap_block_layout_t create_heap_block_layout(const vierkant::DevicePtr &device, const descriptor_map_t &descriptors);

/**
 * @brief   find_or_create_heap_block_layout retrieves a block-layout from a cache, or creates and inserts it.
 *          the cache-key ignores resources, views, buffer-ranges and the length of a variable-count binding.
 *
 * @param   device      handle for a vierkant::Device
 * @param   descriptors a provided descriptor-map
 * @param   cache       cache of block-layouts
 * @return  a shared block-layout for @p descriptors
 */
HeapBlockLayoutPtr find_or_create_heap_block_layout(const vierkant::DevicePtr &device, descriptor_map_t descriptors,
                                                    std::unordered_map<descriptor_map_t, HeapBlockLayoutPtr> &cache);

/**
 * @brief   heap_block_size returns the block-sizes for a descriptor-map, including a variable-count binding.
 *
 * @param   layout      the block-layout for @p descriptors
 * @param   descriptors a provided descriptor-map
 * @return  block-sizes in bytes
 */
heap_block_size_t heap_block_size(const heap_block_layout_t &layout, const descriptor_map_t &descriptors);

/**
 * @brief   heap_push_offset returns the push-data offset of the first heap_block_t, past all push-constants.
 *
 * @param   push_constant_ranges    the push-constant ranges of a pipeline
 * @return  byte-offset in push-data
 */
uint32_t heap_push_offset(const std::vector<VkPushConstantRange> &push_constant_ranges);

/**
 * @brief   create_heap_mappings returns the shader-mappings for one descriptor-set.
 *
 * @param   device      handle for a vierkant::Device
 * @param   layout      the block-layout of the descriptor-set
 * @param   set         the descriptor-set index in the shader
 * @param   push_offset byte-offset of this set's heap_block_t in push-data
 * @return  one mapping per binding
 */
std::vector<VkDescriptorSetAndBindingMappingEXT> create_heap_mappings(const vierkant::DevicePtr &device,
                                                                      const heap_block_layout_t &layout, uint32_t set,
                                                                      uint32_t push_offset);

/**
 * @brief   write_heap_block allocates a block from both arenas and writes all descriptors into it.
 *          like update_descriptor_set, null buffers and images are skipped.
 *
 * @param   device          handle for a vierkant::Device
 * @param   layout          the block-layout for @p descriptors
 * @param   descriptors     a provided descriptor-map
 * @param   resource_arena  an arena of the resource-heap
 * @param   sampler_arena   an arena of the sampler-heap
 * @return  the block-indices to push
 */
heap_block_t write_heap_block(const vierkant::DevicePtr &device, const heap_block_layout_t &layout,
                              const descriptor_map_t &descriptors, DescriptorArena &resource_arena,
                              DescriptorArena &sampler_arena);

/**
 * @brief   HeapBlockAllocator writes blocks into arenas of a DescriptorHeap. the arenas grow on demand.
 *          all blocks are released at once with reset(). not thread-safe.
 */
class HeapBlockAllocator
{
public:
    HeapBlockAllocator() = default;

    explicit HeapBlockAllocator(DescriptorHeapPtr heap);

    /**
     * @brief   write allocates a block and writes all descriptors into it.
     *
     * @param   layout      the block-layout for @p descriptors
     * @param   descriptors a provided descriptor-map
     * @return  the block-indices to push
     */
    heap_block_t write(const heap_block_layout_t &layout, const descriptor_map_t &descriptors);

    /**
     * @brief   release all blocks. the GPU must be done with them.
     */
    void reset();

    [[nodiscard]] const DescriptorHeapPtr &heap() const { return m_heap; }

private:
    DescriptorArena &arena(DescriptorHeapType type, VkDeviceSize num_bytes, VkDeviceSize alignment);

    DescriptorHeapPtr m_heap;

    //! arenas per heap-type, allocations go to the last one
    std::array<std::vector<DescriptorArenaPtr>, 2> m_arenas;
};

}// namespace vierkant
