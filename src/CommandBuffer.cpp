//
// Created by crocdialer on 9/29/18.
//

#include "vierkant/CommandBuffer.hpp"

namespace vierkant {

SemaphorePtr create_semaphore(vierkant::DevicePtr device)
{
    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    vkCreateSemaphore(device->handle(), &semaphore_create_info, nullptr, &semaphore);
    return std::shared_ptr<VkSemaphore_T>(semaphore, [device](VkSemaphore s)
    {
        vkDestroySemaphore(device->handle(), s, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

FencePtr create_fence(vierkant::DevicePtr device, bool signaled)
{
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = nullptr;
    fence_create_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    vkCreateFence(device->handle(), &fence_create_info, nullptr, &fence);

    return FencePtr(fence, [device](VkFence f)
    {
        vkDestroyFence(device->handle(), f, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::CommandBuffer(DevicePtr the_device, VkCommandPool the_pool, VkCommandBufferLevel level) :
        m_device(std::move(the_device)),
        m_handle(VK_NULL_HANDLE),
        m_pool(the_pool),
        m_recording(false)
{
    if(the_pool)
    {
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = level;
        alloc_info.commandPool = the_pool;
        alloc_info.commandBufferCount = 1;

        vkAllocateCommandBuffers(m_device->handle(), &alloc_info, &m_handle);
    }

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = 0;
    vkCreateFence(m_device->handle(), &fence_create_info, nullptr, &m_fence);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::CommandBuffer(CommandBuffer &&the_other) noexcept:
        CommandBuffer()
{
    swap(*this, the_other);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::~CommandBuffer()
{
    if(m_device)
    {
        if(m_recording){ end(); }
        vkDestroyFence(m_device->handle(), m_fence, nullptr);
        if(m_handle && m_pool){ vkFreeCommandBuffers(m_device->handle(), m_pool, 1, &m_handle); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer &CommandBuffer::operator=(CommandBuffer the_other)
{
    swap(*this, the_other);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::begin(VkCommandBufferUsageFlags flags, VkCommandBufferInheritanceInfo *inheritance)
{
    if(m_handle)
    {
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = flags;
        begin_info.pInheritanceInfo = inheritance;
        vkCheck(vkBeginCommandBuffer(m_handle, &begin_info), "failed to begin command buffer!");
        m_recording = true;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::end()
{
    if(m_handle)
    {
        vkCheck(vkEndCommandBuffer(m_handle), "failed to record command buffer!");
        m_recording = false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::submit(VkQueue queue,
                           bool create_fence,
                           VkFence fence,
                           VkSubmitInfo submit_info)
{
    if(m_recording){ end(); }

    if(m_handle && queue)
    {
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_handle;

        if(create_fence)
        {
            vkResetFences(m_device->handle(), 1, &m_fence);
            fence = m_fence;
        }

        vkQueueSubmit(queue, 1, &submit_info, fence);

        if(create_fence)
        {
            vkWaitForFences(m_device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::reset(bool release_resources)
{
    if(m_handle)
    {
        VkCommandBufferResetFlags resetFlags = release_resources ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0;
        vkCheck(vkResetCommandBuffer(m_handle, resetFlags), "failed to reset command buffer");
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(CommandBuffer &lhs, CommandBuffer &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_pool, rhs.m_pool);
    std::swap(lhs.m_handle, rhs.m_handle);
    std::swap(lhs.m_fence, rhs.m_fence);
    std::swap(lhs.m_recording, rhs.m_recording);
}

}//namespace vulkan