//
// Created by crocdialer on 9/26/18.
//

#pragma once

#include "vierkant/Device.hpp"
#include "vierkant/CommandBuffer.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Buffer);

class Buffer
{
public:

    /**
     * @brief   Create a memory pool, that can be used to allocate Buffers from.
     *
     * @param   device          handle for the vierkant::Device to create the pool with
     * @param   usage_flags     the usage-flags for Buffers allocated from this pool
     * @param   mem_usage       the intended memory usage
     * @param   block_size      optional parameter for fixed block-sizes in bytes.
     * @param   min_block_count optional parameter for minimum number of allocated blocks
     * @param   max_block_count optional parameter for maximum number of allocated blocks
     * @param   vma_flags       the VmaPoolCreateFlags. can be used to change the memory-pool's allocation strategy
     * @return  the newly created VmaPoolPtr
     */
    static VmaPoolPtr create_pool(const DevicePtr &device, VkBufferUsageFlags usage_flags, VmaMemoryUsage mem_usage,
                                  VkDeviceSize block_size = 0, size_t min_block_count = 0, size_t max_block_count = 0,
                                  VmaPoolCreateFlags vma_flags = 0);

    static BufferPtr create(DevicePtr device, const void *data, size_t num_bytes,
                            VkBufferUsageFlags usage_flags, VmaMemoryUsage mem_usage,
                            VmaPoolPtr pool = nullptr);

    template<class T>
    static BufferPtr create(DevicePtr the_device, const T &the_array, VkBufferUsageFlags the_usage_flags,
                            VmaMemoryUsage mem_usage, VmaPoolPtr pool = nullptr)
    {
        size_t num_bytes = the_array.size() * sizeof(typename T::value_type);
        return create(the_device, the_array.data(), num_bytes, the_usage_flags, mem_usage, pool);
    }

    Buffer(Buffer &&other) = delete;

    Buffer(const Buffer &other) = delete;

    Buffer &operator=(Buffer other) = delete;

    ~Buffer();

    /**
     * @return true, if the underlying memory-type is host visible and can be mapped
     */
    bool is_host_visible() const;

    /**
     * @brief   map buffer to local memory
     *
     * @return  a pointer to the mapped memory, if successful, nullptr otherwise
     */
    void *map();

    /**
     * @brief   unmap the previously mapped buffer
     */
    void unmap();

    /**
     * @return  the underlying VkBuffer handle
     */
    VkBuffer handle() const;

    /**
     * @return  the VkBufferUsageFlags this buffer was created with
     */
    VkBufferUsageFlags usage_flags() const;

    /**
     * @return  the number of bytes contained in the buffer
     */
    size_t num_bytes() const;

    /**
     * @brief   if the buffer was created with 'VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT' returns its address.
     * @return  the VkDeviceAddress for this buffer or 0 if not available.
     */
    VkDeviceAddress device_address() const{ return m_device_address; };

    /**
     * @brief   upload data into the buffer
     *
     * @param   the_data        pointer to data to be uploaded
     * @param   the_num_bytes   the number of bytes to upload
     */
    void set_data(const void *the_data, size_t the_num_bytes);

    /**
     * @brief   convenience template to set the contents of this buffer from a std::vector or std::array
     */
    template<typename T>
    inline void set_data(const T &the_array)
    {
        size_t num_bytes = the_array.size() * sizeof(typename T::value_type);
        set_data((void *) the_array.data(), num_bytes);
    };

    /**
     * @brief   copy the contents of this buffer to another buffer
     *
     * @param   dst     the destination buffer for the copy operation
     * @param   cmd_buf optional pointer to an existing CommandBuffer to be used for the copy operation
     */
    void copy_to(const BufferPtr &dst, VkCommandBuffer cmdBufferHandle = VK_NULL_HANDLE);

    /**
     * @return  the vierkant::DevicePtr used to create the buffer.
     */
    vierkant::DevicePtr device() const{ return m_device; }

private:

    DevicePtr m_device;

    VkBuffer m_buffer = VK_NULL_HANDLE;

    void *m_mapped_data = nullptr;

    VkDeviceAddress m_device_address = 0;

    VmaAllocation m_allocation = nullptr;

    VmaAllocationInfo m_allocation_info = {};

    size_t m_num_bytes = 0;

    VkBufferUsageFlags m_usage = 0;

    VmaMemoryUsage m_mem_usage = VMA_MEMORY_USAGE_UNKNOWN;

    VmaPoolPtr m_pool = nullptr;

    Buffer(DevicePtr the_device, VkBufferUsageFlags the_usage_flags, VmaMemoryUsage mem_usage, VmaPoolPtr pool);
};

}//namespace vulkan