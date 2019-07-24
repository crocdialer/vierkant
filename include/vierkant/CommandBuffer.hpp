//
// Created by crocdialer on 9/29/18.
//

#pragma once

#include "vierkant/Device.hpp"

namespace vierkant {

using SemaphorePtr = std::shared_ptr<VkSemaphore_T>;

/**
 * @brief   Create a ref counted semaphore object
 *
 * @param   device  the VkDevice used to create the Semaphore
 * @return  a newly created Semaphore
 */
SemaphorePtr create_semaphore(vierkant::DevicePtr device);

using FencePtr = std::shared_ptr<VkFence_T>;

/**
 * @brief   Create a ref counted fence object
 *
 * @param   device      the VkDevice used to create the Fence
 * @param   signaled    flag indicating if the fence should be created in signaled state
 * @return  a newly created Fence
 */
FencePtr create_fence(vierkant::DevicePtr device, bool signaled = false);

/**
 * @brief   Submit an array of VkCommandBuffer to a VkQueue.
 *
 * @param   device          shared handle to a VkDevice
 * @param   queue           a VkQueue to submit the commands to
 * @param   commandBuffers  an array of recorded VkCommandBuffers
 * @param   fence           an optional fence to signal
 * @param   wait_fence      optional flag indicating that the fence should be waited on
 * @param   submitInfo      an optional VkSubmitInfo that can be used to pass in signal/wait semaphores
 */
void submit(const vierkant::DevicePtr& device,
            VkQueue queue,
            const std::vector<VkCommandBuffer>& commandBuffers,
            VkFence fence = VK_NULL_HANDLE,
            bool wait_fence = false,
            VkSubmitInfo submitInfo = {});

using CommandPoolPtr = std::shared_ptr<VkCommandPool_T>;

class CommandBuffer
{
public:

    /**
     * @brief   construct a new CommandBuffer
     *
     * @param   the_device  the VkDevice that should be used to create the CommandBuffer
     * @param   the_pool    the VkCommandPool to allocate the CommandBuffer from
     * @param   level       the VkCommandBufferLevel
     * @param   the_queue   an optional VkQueue to automatically submit the CommandBuffer before destruction
     */
    CommandBuffer(DevicePtr the_device,
                  VkCommandPool the_pool,
                  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    CommandBuffer() = default;

    CommandBuffer(CommandBuffer &&the_other) noexcept;

    CommandBuffer(const CommandBuffer &) = delete;

    ~CommandBuffer();

    CommandBuffer &operator=(CommandBuffer the_other);

    /**
     * @brief   start recording commands into buffer
     *
     * @param   flags       bitmask for usage flags
     * @param   inheritance optional pointer to a VkCommandBufferInheritanceInfo
     */
    void begin(VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
               VkCommandBufferInheritanceInfo *inheritance = nullptr);

    /**
     * @brief   stop recording commands into this buffer
     */
    void end();

    /**
     * @brief   Submit the commandbuffer to specified queue.
     *
     * @param   queue           VkQueue to submit this CommandBuffer to
     * @param   create_fence    flag indicating if synchronization (blocking wait) via internal fence should be done.
     * @param   fence           optional external VkFence object to wait on
     * @param   submit_info     optional VkSubmitInfo struct. can be used to provide synchronization -semaphores
     *                          and stage-info
     */
    void submit(VkQueue queue,
                bool create_fence = false,
                VkFence fence = VK_NULL_HANDLE,
                VkSubmitInfo submit_info = {});

    /**
     * @brief   Reset the CommandBuffer back to an initial state,
     *          optionally freeing allocated resources
     *
     * @param   release_resources    flag indicating wether to free all allocated resources
     */
    void reset(bool release_resources = false);

    VkCommandBuffer handle() const { return m_handle; }

    VkCommandPool pool() const { return m_pool; }

    bool is_recording() const { return m_recording; }

    inline explicit operator bool() const { return static_cast<bool>(m_handle); };

    friend void swap(CommandBuffer &lhs, CommandBuffer &rhs);

private:
    DevicePtr m_device = nullptr;
    VkCommandBuffer m_handle = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
    VkCommandPool m_pool = VK_NULL_HANDLE;
    bool m_recording = false;
};

}//namespace vulkan