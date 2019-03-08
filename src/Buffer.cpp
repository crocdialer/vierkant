//
// Created by crocdialer on 9/26/18.
//

#include "../include/vierkant/Buffer.hpp"

namespace vierkant {

///////////////////////////////////////////////////////////////////////////////////////////////////

VmaMemoryUsage get_vma_memory_usage(VkMemoryPropertyFlags the_props)
{
    if(the_props & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT){ return VMA_MEMORY_USAGE_GPU_ONLY; }
    else if(the_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        if(the_props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT){ return VMA_MEMORY_USAGE_CPU_ONLY; }
        else{ return VMA_MEMORY_USAGE_CPU_TO_GPU; }
    }
    return VMA_MEMORY_USAGE_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void copy_to_helper(const DevicePtr &device, Buffer *src, Buffer *dst, VkCommandBuffer cmdBufferHandle = VK_NULL_HANDLE)
{
    CommandBuffer local_cmd_buf;

    if(!cmdBufferHandle)
    {
        local_cmd_buf = CommandBuffer(device, device->command_pool_transfer());
        local_cmd_buf.begin();
        cmdBufferHandle = local_cmd_buf.handle();
    }
    // assure dst buffer has correct size, no-op if already the case
    dst->set_data(nullptr, src->num_bytes());

    VkBufferCopy copy_region = {};
    copy_region.srcOffset = 0; // Optional
    copy_region.dstOffset = 0; // Optional
    copy_region.size = src->num_bytes();
    vkCmdCopyBuffer(cmdBufferHandle, src->handle(), dst->handle(), 1, &copy_region);

    if(local_cmd_buf)
    {
        // submit command buffer, also creates a fence and waits for the operation to complete
        local_cmd_buf.submit(device->transfer_queue(), true);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BufferPtr
Buffer::create(DevicePtr the_device, const void *the_data, size_t the_num_bytes, VkBufferUsageFlags the_usage_flags,
               VkMemoryPropertyFlags the_properties)
{
    auto ret = BufferPtr(new Buffer(std::move(the_device), the_usage_flags, the_properties));
    ret->set_data(the_data, the_num_bytes);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Buffer::Buffer(DevicePtr the_device, uint32_t the_usage_flags, VkMemoryPropertyFlags the_properties) :
        m_device(std::move(the_device)),
        m_usage(the_usage_flags),
        m_mem_properties(the_properties)
{

}

Buffer::~Buffer()
{
    vmaDestroyBuffer(m_device->vk_mem_allocator(), m_buffer, m_allocation);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Buffer::is_host_visible() const
{
    return m_mem_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void *Buffer::map()
{
    void *ret = nullptr;
    if(is_host_visible() && vmaMapMemory(m_device->vk_mem_allocator(), m_allocation, &ret) == VK_SUCCESS)
    {
        return ret;
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Buffer::unmap()
{
    vmaUnmapMemory(m_device->vk_mem_allocator(), m_allocation);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkBuffer Buffer::handle() const
{
    return m_buffer;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkBufferUsageFlags Buffer::usage_flags() const
{
    return m_usage;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t Buffer::num_bytes() const
{
    return m_num_bytes;
}

void Buffer::set_data(const void *the_data, size_t the_num_bytes)
{
    if(!the_num_bytes){ return; }

    if(m_num_bytes != the_num_bytes)
    {
        if(m_buffer){ vmaDestroyBuffer(m_device->vk_mem_allocator(), m_buffer, m_allocation); }

        // create buffer
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = the_num_bytes;
        buffer_info.usage = m_usage;
        if(!is_host_visible() && the_data){ buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = get_vma_memory_usage(m_mem_properties);

        vmaCreateBuffer(m_device->vk_mem_allocator(), &buffer_info, &alloc_info, &m_buffer, &m_allocation,
                        &m_allocation_info);

        // the actually allocated num_bytes might be bigger
        m_num_bytes = the_num_bytes;
    }

    if(the_data)
    {
        void *buf_data = map();

        if(buf_data)
        {
            memcpy(buf_data, the_data, the_num_bytes);
            unmap();
        }else
        {
            // create staging buffer
            auto staging_buffer = Buffer::create(m_device, the_data, the_num_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            // copy staging buffer to this buffer
            copy_to_helper(m_device, staging_buffer.get(), this, nullptr);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Buffer::copy_to(BufferPtr dst, VkCommandBuffer cmdBufferHandle)
{
    copy_to_helper(m_device, this, dst.get(), cmdBufferHandle);
}

}//namespace vulkan
