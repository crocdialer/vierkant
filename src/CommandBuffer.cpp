//
// Created by crocdialer on 9/29/18.
//

#include "vierkant/CommandBuffer.hpp"

namespace vierkant
{

FencePtr create_fence(const vierkant::DevicePtr &device, bool signaled)
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wait_fence(const vierkant::DevicePtr &device, const vierkant::FencePtr &fence, bool reset)
{
    // wait for prior fence
    VkFence handle = fence.get();

    if(handle)
    {
        vkWaitForFences(device->handle(), 1, &handle, VK_TRUE, std::numeric_limits<uint64_t>::max());
        if(reset){ vkResetFences(device->handle(), 1, &handle); }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void submit(const vierkant::DevicePtr &device,
            VkQueue queue,
            const std::vector<VkCommandBuffer> &command_buffers,
            bool wait_fence,
            VkFence fence,
            const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos)
{
    if(device && queue)
    {
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.commandBufferCount = command_buffers.size();
        submit_info.pCommandBuffers = command_buffers.data();

        if(semaphore_infos.empty()){ vkQueueSubmit(queue, 1, &submit_info, fence); }
        else
        {
            // submit with synchronization-infos
            std::vector<VkSemaphore> wait_semaphores;
            std::vector<VkSemaphore> signal_semaphores;
            std::vector<VkPipelineStageFlags> wait_stages;
            std::vector<uint64_t> wait_values;
            std::vector<uint64_t> signal_values;

            for(const auto &semaphore_info : semaphore_infos)
            {
                if(semaphore_info.semaphore)
                {
                    if(semaphore_info.wait_stage)
                    {
                        wait_semaphores.push_back(semaphore_info.semaphore);
                        wait_stages.push_back(semaphore_info.wait_stage);
                        wait_values.push_back(semaphore_info.wait_value);
                    }
                    if(semaphore_info.signal_value)
                    {
                        signal_semaphores.push_back(semaphore_info.semaphore);
                        signal_values.push_back(semaphore_info.signal_value);
                    }
                }
            }

            VkTimelineSemaphoreSubmitInfo timeline_info;
            timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timeline_info.pNext = nullptr;
            timeline_info.waitSemaphoreValueCount = wait_values.size();
            timeline_info.pWaitSemaphoreValues = wait_values.data();
            timeline_info.signalSemaphoreValueCount = signal_values.size();
            timeline_info.pSignalSemaphoreValues = signal_values.data();

            submit_info.pNext = &timeline_info;
            submit_info.waitSemaphoreCount = wait_semaphores.size();
            submit_info.pWaitSemaphores = wait_semaphores.data();
            submit_info.pWaitDstStageMask = wait_stages.data();
            submit_info.signalSemaphoreCount = signal_semaphores.size();
            submit_info.pSignalSemaphores = signal_semaphores.data();

            vierkant::FencePtr local_fence;
            if(wait_fence && !fence)
            {
                local_fence = vierkant::create_fence(device);
                fence = local_fence.get();
            }

            vkQueueSubmit(queue, 1, &submit_info, fence);
        }

        if(wait_fence && fence)
        {
            vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CommandPoolPtr create_command_pool(const vierkant::DevicePtr &device, vierkant::Device::Queue queue_type,
                                   VkCommandPoolCreateFlags flags)
{
    // command pool
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // transient command pool -> graphics queue
    auto queue_indices = device->queue_family_indices();
    pool_info.queueFamilyIndex = static_cast<uint32_t>(queue_indices[queue_type].index);
    pool_info.flags = flags;
    VkCommandPool command_pool;
    vkCheck(vkCreateCommandPool(device->handle(), &pool_info, nullptr, &command_pool),
            "failed to create command pool!");

    return vierkant::CommandPoolPtr(command_pool, [device](VkCommandPool pool)
    {
        vkDestroyCommandPool(device->handle(), pool, nullptr);
    });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::CommandBuffer(DevicePtr device, VkCommandPool command_pool, VkCommandBufferLevel level) :
        m_device(std::move(device)),
        m_handle(VK_NULL_HANDLE),
        m_pool(command_pool),
        m_recording(false)
{
    if(command_pool)
    {
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = level;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        vkAllocateCommandBuffers(m_device->handle(), &alloc_info, &m_handle);
    }

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = 0;
    vkCreateFence(m_device->handle(), &fence_create_info, nullptr, &m_fence);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::CommandBuffer(CommandBuffer &&other) noexcept:
        CommandBuffer()
{
    swap(*this, other);
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

CommandBuffer &CommandBuffer::operator=(CommandBuffer other)
{
    swap(*this, other);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::begin(VkCommandBufferUsageFlags flags, VkCommandBufferInheritanceInfo *inheritance)
{
    if(m_handle)
    {
        if(inheritance && inheritance->renderPass &&
           inheritance->framebuffer){ flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT; }
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
                           bool wait_fence,
                           VkFence fence,
                           const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos)
{
    if(m_recording){ end(); }

    if(m_handle && queue)
    {
        if(wait_fence)
        {
            vkResetFences(m_device->handle(), 1, &m_fence);
            fence = m_fence;
        }

        if(semaphore_infos.empty())
        {
            VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &m_handle;

            vkQueueSubmit(queue, 1, &submit_info, fence);

            if(wait_fence)
            {
                vkWaitForFences(m_device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
            }
        }
        else
        {
            vierkant::submit(m_device, queue, {m_handle}, wait_fence, fence, semaphore_infos);
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