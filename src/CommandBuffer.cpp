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

    return {fence, [device](VkFence f) { vkDestroyFence(device->handle(), f, nullptr); }};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void wait_fence(const vierkant::DevicePtr &device, const vierkant::FencePtr &fence, bool reset)
{
    // wait for prior fence
    VkFence handle = fence.get();

    if(handle)
    {
        vkWaitForFences(device->handle(), 1, &handle, VK_TRUE, std::numeric_limits<uint64_t>::max());
        if(reset) { vkResetFences(device->handle(), 1, &handle); }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void submit(const vierkant::DevicePtr &device, VkQueue queue, const std::vector<VkCommandBuffer> &command_buffers,
            bool wait_fence, VkFence fence, const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos)
{
    if(device && queue)
    {
        vierkant::FencePtr local_fence;
        if(fence) { vkResetFences(device->handle(), 1, &fence); }

        std::vector<VkCommandBufferSubmitInfo> command_submit_infos(command_buffers.size());
        for(uint32_t i = 0; i < command_buffers.size(); ++i)
        {
            auto &cmd_info = command_submit_infos[i];
            cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            cmd_info.commandBuffer = command_buffers[i];
            cmd_info.deviceMask = 0;
        }

        VkSubmitInfo2 submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.commandBufferInfoCount = command_submit_infos.size();
        submit_info.pCommandBufferInfos = command_submit_infos.data();

        auto queue_asset = device->queue_asset(queue);
        assert(queue_asset);

        if(semaphore_infos.empty())
        {
            std::unique_lock lock(*queue_asset->mutex);
            vkQueueSubmit2(queue, 1, &submit_info, fence);
        }
        else
        {
            // submit with synchronization-infos
            std::vector<VkSemaphoreSubmitInfo> wait_semaphores;
            std::vector<VkSemaphoreSubmitInfo> signal_semaphores;

            for(const auto &semaphore_info: semaphore_infos)
            {
                if(semaphore_info.semaphore)
                {
                    VkSemaphoreSubmitInfo semaphore_submit_info = {};
                    semaphore_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                    semaphore_submit_info.semaphore = semaphore_info.semaphore;
                    semaphore_submit_info.deviceIndex = 0;

                    if(semaphore_info.wait_stage)
                    {
                        semaphore_submit_info.stageMask = semaphore_info.wait_stage;
                        semaphore_submit_info.value = semaphore_info.wait_value;
                        wait_semaphores.push_back(semaphore_submit_info);
                    }

                    if(semaphore_info.signal_value)
                    {
                        semaphore_submit_info.stageMask = semaphore_info.signal_stage;
                        semaphore_submit_info.value = semaphore_info.signal_value;
                        signal_semaphores.push_back(semaphore_submit_info);
                    }
                }
            }

            submit_info.signalSemaphoreInfoCount = signal_semaphores.size();
            submit_info.pSignalSemaphoreInfos = signal_semaphores.data();
            submit_info.waitSemaphoreInfoCount = wait_semaphores.size();
            submit_info.pWaitSemaphoreInfos = wait_semaphores.data();

            if(wait_fence && !fence)
            {
                local_fence = vierkant::create_fence(device);
                fence = local_fence.get();
            }

            std::unique_lock lock(*queue_asset->mutex);
            vkQueueSubmit2(queue, 1, &submit_info, fence);
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

    return {command_pool, [device](VkCommandPool pool) { vkDestroyCommandPool(device->handle(), pool, nullptr); }};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::CommandBuffer(DevicePtr device, VkCommandPool command_pool)
    : CommandBuffer({.device = std::move(device), .command_pool = command_pool, .name = ""})
{}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::CommandBuffer(const create_info_t &create_info)
    : m_device(create_info.device), m_handle(VK_NULL_HANDLE), m_pool(create_info.command_pool), m_recording(false)
{
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = create_info.level;
    alloc_info.commandPool = create_info.command_pool;
    alloc_info.commandBufferCount = 1;
    vkCheck(vkAllocateCommandBuffers(m_device->handle(), &alloc_info, &m_handle), "failed to create command buffer!");

    //! set optional name for debugging
    if(!create_info.name.empty())
    {
        m_device->set_object_name(uint64_t(m_handle), VK_OBJECT_TYPE_COMMAND_BUFFER, create_info.name);
    }

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = 0;
    vkCreateFence(m_device->handle(), &fence_create_info, nullptr, &m_fence);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::CommandBuffer(CommandBuffer &&other) noexcept : CommandBuffer() { swap(*this, other); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer::~CommandBuffer()
{
    if(m_device)
    {
        if(m_recording) { end(); }
        vkDestroyFence(m_device->handle(), m_fence, nullptr);
        if(m_handle && m_pool) { vkFreeCommandBuffers(m_device->handle(), m_pool, 1, &m_handle); }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandBuffer &CommandBuffer::operator=(CommandBuffer other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::begin(VkCommandBufferUsageFlags flags, VkCommandBufferInheritanceInfo *inheritance)
{
    if(m_handle)
    {
        if(inheritance && inheritance->renderPass && inheritance->framebuffer)
        {
            flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        }
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = flags;
        begin_info.pInheritanceInfo = inheritance;
        vkCheck(vkBeginCommandBuffer(m_handle, &begin_info), "failed to begin command buffer!");
        m_recording = true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::end()
{
    if(m_handle)
    {
        vkCheck(vkEndCommandBuffer(m_handle), "failed to record command buffer!");
        m_recording = false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::submit(VkQueue queue, bool wait_fence, VkFence fence,
                           const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos)
{
    if(m_recording) { end(); }

    if(m_handle && queue)
    {
        if(wait_fence)
        {
            vkResetFences(m_device->handle(), 1, &m_fence);
            fence = m_fence;
        }
        vierkant::submit(m_device, queue, {m_handle}, wait_fence, fence, semaphore_infos);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandBuffer::reset(bool release_resources)
{
    if(m_handle)
    {
        VkCommandBufferResetFlags resetFlags = release_resources ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0;
        vkCheck(vkResetCommandBuffer(m_handle, resetFlags), "failed to reset command buffer");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void swap(CommandBuffer &lhs, CommandBuffer &rhs)
{
    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_pool, rhs.m_pool);
    std::swap(lhs.m_handle, rhs.m_handle);
    std::swap(lhs.m_fence, rhs.m_fence);
    std::swap(lhs.m_recording, rhs.m_recording);
}

}// namespace vierkant