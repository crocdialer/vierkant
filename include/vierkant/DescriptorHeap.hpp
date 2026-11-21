//
// Created by crocdialer on 11/19/26.
//

#pragma once

#include <array>
#include <mutex>
#include <unordered_map>
#include <vierkant/Buffer.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(DescriptorHeap)
DEFINE_CLASS_PTR(DescriptorRange)

//! round up to a multiple of alignment
constexpr VkDeviceSize align_up(const VkDeviceSize value, const VkDeviceSize alignment)
{ return (value + alignment - 1) / alignment * alignment; }

//! descriptor-heaps come in two types, bound separately
enum class DescriptorHeapType : uint32_t
{
    RESOURCE = 0,
    SAMPLER = 1
};

//! one sampler in a sampler-block
struct sampler_slot_t
{
    //! byte-offset in the block
    uint32_t offset = 0;
    sampler_state_t state = {};

    bool operator==(const sampler_slot_t &other) const = default;
};

/**
 * @brief   DescriptorHeap owns a resource- and a sampler-heap (VK_EXT_descriptor_heap).
 *          each heap is a host-visible buffer. it starts with the reserved range owned by the implementation,
 *          the remaining space is handed out as DescriptorRanges.
 */
class DescriptorHeap : public std::enable_shared_from_this<DescriptorHeap>
{
public:
    struct create_info_t
    {
        DevicePtr device;

        //! requested heap-size in bytes, including the reserved range. clamped to the device-limit.
        VkDeviceSize resource_heap_size = 32 << 20;

        //! requested heap-size in bytes, including the reserved range. clamped to the device-limit.
        VkDeviceSize sampler_heap_size = 128 << 10;

        //! optional name for debugging
        std::string name;
    };

    /**
     * @return  the device-extensions required by DescriptorHeap
     */
    static std::vector<const char *> required_extensions();

    /**
     * @brief   Create a shared DescriptorHeap.
     *          the device must be created with the extensions from DescriptorHeap::required_extensions().
     *
     * @param   create_info a provided create_info_t
     * @return  the newly created DescriptorHeapPtr
     */
    static DescriptorHeapPtr create(const create_info_t &create_info);

    DescriptorHeap(const DescriptorHeap &) = delete;

    DescriptorHeap(DescriptorHeap &&) = delete;

    DescriptorHeap &operator=(DescriptorHeap other) = delete;

    ~DescriptorHeap();

    /**
     * @brief   create_range reserves a range of a heap. the range returns to the heap when it is destroyed.
     *          thread-safe.
     *
     * @param   type        the heap to reserve from
     * @param   num_bytes   size of the range in bytes
     * @return  a newly created DescriptorRange
     */
    DescriptorRangePtr create_range(DescriptorHeapType type, VkDeviceSize num_bytes);

    /**
     * @brief   sampler_block returns a sampler-block with the given samplers. equal requests share one block.
     *          the block is written once and returns to the heap when the last holder drops it. thread-safe.
     *
     * @param   slots       the samplers and their byte-offsets in the block
     * @param   num_bytes   size of the block in bytes
     * @return  a shared, write-once range of the sampler-heap
     */
    DescriptorRangeConstPtr sampler_block(const std::vector<sampler_slot_t> &slots, uint32_t num_bytes);

    /**
     * @brief   bind both heaps to a command-buffer
     *
     * @param   command_buffer  a command-buffer in recording state
     */
    void bind(VkCommandBuffer command_buffer) const;

    /**
     * @return  the bind-info for a heap, also used for secondary command-buffer inheritance
     */
    [[nodiscard]] const VkBindHeapInfoEXT &bind_info(DescriptorHeapType type) const;

    /**
     * @return  the buffer backing a heap
     */
    [[nodiscard]] const vierkant::BufferPtr &buffer(DescriptorHeapType type) const;

    //! the device this heap was created for
    [[nodiscard]] const vierkant::DevicePtr &device() const { return m_device; }

private:
    struct heap_t
    {
        vierkant::BufferPtr buffer;
        uint8_t *data = nullptr;
        VkBindHeapInfoEXT bind_info = {};

        //! byte-offset of the first range, past the reserved range
        VkDeviceSize range_offset = 0;

        //! required alignment for ranges
        VkDeviceSize range_alignment = 1;

        //! sub-allocates the space past the reserved range
        VmaVirtualBlock virtual_block = VK_NULL_HANDLE;
    };

    struct sampler_block_key_t
    {
        uint32_t num_bytes = 0;
        std::vector<sampler_slot_t> slots;

        bool operator==(const sampler_block_key_t &other) const = default;
    };

    struct sampler_block_key_hash_t
    { size_t operator()(const sampler_block_key_t &key) const; };

    explicit DescriptorHeap(const create_info_t &create_info);

    void free_range(DescriptorHeapType type, VmaVirtualAllocation allocation);

    vierkant::DevicePtr m_device;

    std::array<heap_t, 2> m_heaps;

    // guards the virtual blocks
    std::mutex m_mutex;

    //! live sampler-blocks, entries expire with their last holder
    std::unordered_map<sampler_block_key_t, std::weak_ptr<const DescriptorRange>, sampler_block_key_hash_t>
            m_sampler_blocks;

    // guards m_sampler_blocks
    std::mutex m_sampler_mutex;
};

/**
 * @brief   DescriptorRange is a range of a DescriptorHeap, owned by a single user.
 *          the range returns to the heap when it is destroyed.
 */
class DescriptorRange
{
public:
    DescriptorRange(const DescriptorRange &) = delete;

    DescriptorRange(DescriptorRange &&) = delete;

    DescriptorRange &operator=(DescriptorRange other) = delete;

    [[nodiscard]] DescriptorHeapType type() const { return m_type; }

    //! byte-offset of this range from the start of the heap
    [[nodiscard]] VkDeviceSize offset() const { return m_offset; }

    //! size of this range in bytes
    [[nodiscard]] VkDeviceSize num_bytes() const { return m_num_bytes; }

    //! host-address of the range-start
    [[nodiscard]] uint8_t *data() const { return m_data; }

private:
    friend class DescriptorHeap;

    DescriptorRange(DescriptorHeapPtr heap, DescriptorHeapType type, VkDeviceSize offset, VkDeviceSize num_bytes,
                    uint8_t *data, VmaVirtualAllocation allocation);

    DescriptorHeapPtr m_heap;
    DescriptorHeapType m_type;
    VkDeviceSize m_offset;
    VkDeviceSize m_num_bytes;
    uint8_t *m_data;
    VmaVirtualAllocation m_allocation;
};

}// namespace vierkant
