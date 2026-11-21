//
// Created by crocdialer on 11/20/26.
//

#pragma once

#include <unordered_set>
#include <vierkant/DescriptorHeap.hpp>
#include <vierkant/descriptor.hpp>
#include <vierkant/pipeline_formats.hpp>

namespace vierkant
{

/**
 * @brief   heap_block_layout_t is the layout of one descriptor_map_t in the resource- and sampler-heap.
 *          a block is addressed by a heap_block_ref_t in push-data.
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

    //! resource-block alignment
    uint32_t alignment = 1;
};

//! location of a block, pushed once per descriptor-set
struct heap_block_ref_t
{
    //! byte-offset of the resource-block in the resource-heap
    uint32_t resource_offset = 0;

    //! byte-offset of the sampler-block in the sampler-heap
    uint32_t sampler_offset = 0;
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
 * @brief   cmd_push_heap_data records the push-data of a descriptor-heap pipeline:
 *          push-constants at offset 0, then one heap_block_ref_t per set. throws if the push-constants overlap them.
 *
 * @param   command_buffer          a command-buffer in recording state
 * @param   push_constant_ranges    the push-constant ranges the pipeline was created with
 * @param   push_constants          push-constant bytes, may be empty
 * @param   blocks                  block-refs, one per set
 */
void cmd_push_heap_data(VkCommandBuffer command_buffer, const std::vector<VkPushConstantRange> &push_constant_ranges,
                        std::span<const uint8_t> push_constants, std::span<const heap_block_ref_t> blocks);

/**
 * @brief   create_heap_mappings returns the shader-mappings for all descriptor-sets of a pipeline.
 *          the block-refs follow the push-constants, one per set.
 *
 * @param   device                  handle for a vierkant::Device
 * @param   layouts                 the block-layouts, one per descriptor-set
 * @param   push_constant_ranges    the push-constant ranges of the pipeline
 * @return  one mapping per binding
 */
std::vector<VkDescriptorSetAndBindingMappingEXT>
create_heap_mappings(const vierkant::DevicePtr &device, const std::vector<HeapBlockLayoutPtr> &layouts,
                     const std::vector<VkPushConstantRange> &push_constant_ranges);

/**
 * @brief   HeapBlockAllocator writes blocks into ranges of a DescriptorHeap. the ranges grow on demand.
 *          samplers come from shared sampler-blocks of the heap, held until reset().
 *          like update_descriptor_set, null buffers and images are skipped.
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
     * @return  the block-ref to push
     */
    heap_block_ref_t write(const heap_block_layout_t &layout, const descriptor_map_t &descriptors);

    /**
     * @brief   release all blocks. the GPU must be done with them.
     */
    void reset();

private:
    //! returns the heap-offset of @p num_bytes in the last range, adds a larger range if it is full
    VkDeviceSize allocate(VkDeviceSize num_bytes, VkDeviceSize alignment);

    DescriptorHeapPtr m_heap;

    //! resource-ranges, allocations go to the last one
    std::vector<DescriptorRangePtr> m_ranges;

    //! bytes in use in the last range
    VkDeviceSize m_num_bytes_used = 0;

    //! sampler-blocks used since the last reset()
    std::unordered_set<DescriptorRangeConstPtr> m_sampler_blocks;
};

}// namespace vierkant
