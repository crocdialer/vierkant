#define VK_NO_PROTOTYPES
#include <volk.h>

#include "vierkant/Buffer.hpp"

namespace vierkant
{

///////////////////////////////////////////////////////////////////////////////////////////////////

void copy_to_helper(const DevicePtr &device, Buffer *src, Buffer *dst, VkCommandBuffer cmd_handle = VK_NULL_HANDLE,
                    size_t src_offset = 0, size_t dst_offset = 0, size_t num_bytes = 0)
{
    if(!num_bytes) { num_bytes = src->num_bytes(); }

    assert(src_offset + num_bytes <= src->num_bytes());

    CommandBuffer local_cmd_buf;

    if(!cmd_handle)
    {
        local_cmd_buf = CommandBuffer({device, device->command_pool_transfer()});
        local_cmd_buf.begin();
        cmd_handle = local_cmd_buf.handle();
    }
    // assure dst buffer has correct size, no-op if already the case
    dst->set_data(nullptr, dst_offset + num_bytes);

    VkBufferCopy2 copy_region = {};
    copy_region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
    copy_region.size = num_bytes;
    copy_region.srcOffset = src_offset;
    copy_region.dstOffset = dst_offset;

    VkCopyBufferInfo2 copy_info2 = {};
    copy_info2.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
    copy_info2.srcBuffer = src->handle();
    copy_info2.dstBuffer = dst->handle();
    copy_info2.regionCount = 1;
    copy_info2.pRegions = &copy_region;
    vkCmdCopyBuffer2(cmd_handle, &copy_info2);

    if(local_cmd_buf)
    {
        // submit command buffer, also creates a fence and waits for the operation to complete
        local_cmd_buf.submit(device->queue(Device::Queue::TRANSFER), true);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VmaPoolPtr Buffer::create_pool(const DevicePtr &device, VkBufferUsageFlags usage_flags, VmaMemoryUsage mem_usage,
                               VmaPoolCreateInfo pool_create_info)
{
    VkBufferCreateInfo dummy_buf_create_info = {};
    dummy_buf_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    dummy_buf_create_info.usage = usage_flags;
    dummy_buf_create_info.size = 1024;

    VmaAllocationCreateInfo dummy_alloc_create_info = {};
    dummy_alloc_create_info.usage = mem_usage;
    uint32_t memTypeIndex;
    vmaFindMemoryTypeIndexForBufferInfo(device->vk_mem_allocator(), &dummy_buf_create_info, &dummy_alloc_create_info,
                                        &memTypeIndex);

    pool_create_info.memoryTypeIndex = memTypeIndex;

    // create pool
    VmaPool pool;
    vmaCreatePool(device->vk_mem_allocator(), &pool_create_info, &pool);

    // return self-destructing VmaPoolPtr
    return {pool, [device](VmaPool p) { vmaDestroyPool(device->vk_mem_allocator(), p); }};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BufferPtr Buffer::create(const create_info_t &create_info)
{
    auto ret = BufferPtr(new Buffer(create_info));
    ret->set_data(create_info.data, create_info.num_bytes);
    return ret;
}

BufferPtr Buffer::create(DevicePtr device, const void *data, size_t num_bytes, VkBufferUsageFlags usage_flags,
                         VmaMemoryUsage mem_usage, VmaPoolPtr pool)
{
    Buffer::create_info_t info = {};
    info.device = std::move(device);
    info.data = data;
    info.num_bytes = num_bytes;
    info.usage = usage_flags;
    info.mem_usage = mem_usage;
    info.pool = std::move(pool);
    auto ret = BufferPtr(new Buffer(info));
    ret->set_data(data, num_bytes);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Buffer::Buffer(const create_info_t &create_info)
    : m_device(create_info.device), m_usage(create_info.usage), m_mem_usage(create_info.mem_usage),
      m_pool(create_info.pool), m_name(create_info.name)
{}

Buffer::~Buffer()
{
    // explicitly unmap, if necessary
    unmap();

    // destroy buffer
    if(m_buffer) { vmaDestroyBuffer(m_device->vk_mem_allocator(), m_buffer, m_allocation); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Buffer::is_host_visible() const
{
    return m_mem_usage == VMA_MEMORY_USAGE_CPU_ONLY || m_mem_usage == VMA_MEMORY_USAGE_CPU_TO_GPU ||
           m_mem_usage == VMA_MEMORY_USAGE_GPU_TO_CPU;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void *Buffer::map()
{
    if(m_mapped_data) { return m_mapped_data; }
    else if(is_host_visible() && vmaMapMemory(m_device->vk_mem_allocator(), m_allocation, &m_mapped_data) == VK_SUCCESS)
    {
        return m_mapped_data;
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Buffer::unmap()
{
    if(m_mapped_data)
    {
        vmaUnmapMemory(m_device->vk_mem_allocator(), m_allocation);
        m_mapped_data = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkBuffer Buffer::handle() const { return m_buffer; }

///////////////////////////////////////////////////////////////////////////////////////////////////

VkBufferUsageFlags Buffer::usage_flags() const { return m_usage; }

///////////////////////////////////////////////////////////////////////////////////////////////////

size_t Buffer::num_bytes() const { return m_num_bytes; }

void Buffer::set_data(const void *data, size_t num_bytes)
{
    if(!num_bytes) { return; }

    if(m_num_bytes < num_bytes)
    {
        if(m_buffer)
        {
            unmap();
            vmaDestroyBuffer(m_device->vk_mem_allocator(), m_buffer, m_allocation);
        }

        // create buffer
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = num_bytes;
        buffer_info.usage = m_usage;
        if(!is_host_visible() && data) { buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = m_mem_usage;
        alloc_info.pool = m_pool.get();

        vmaCreateBuffer(m_device->vk_mem_allocator(), &buffer_info, &alloc_info, &m_buffer, &m_allocation,
                        &m_allocation_info);

        //! set optional name for debugging
        if(!m_name.empty()) { m_device->set_object_name(uint64_t(m_buffer), VK_OBJECT_TYPE_BUFFER, m_name); }

        // the actually allocated num_bytes might be bigger
        m_num_bytes = num_bytes;

        // query the VkDeviceAddress for this buffer
        if(m_usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            VkBufferDeviceAddressInfo buf_info = {};
            buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            buf_info.buffer = m_buffer;
            m_device_address = vkGetBufferDeviceAddress(m_device->handle(), &buf_info);
        }
    }

    if(data)
    {
        void *buf_data = map();

        if(buf_data)
        {
            memcpy(buf_data, data, num_bytes);
            unmap();
        }
        else
        {
            // create staging buffer
            auto staging_buffer = Buffer::create(m_device, data, num_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_MEMORY_USAGE_CPU_ONLY);

            // copy staging buffer to this buffer
            copy_to_helper(m_device, staging_buffer.get(), this, nullptr);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Buffer::copy_to(const BufferPtr &dst, VkCommandBuffer cmdBufferHandle, size_t src_offset, size_t dst_offset,
                     size_t num_bytes)
{
    copy_to_helper(m_device, this, dst.get(), cmdBufferHandle, src_offset, dst_offset, num_bytes);
}

}// namespace vierkant
