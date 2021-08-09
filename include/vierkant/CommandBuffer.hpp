//
// Created by crocdialer on 9/29/18.
//

#pragma once

#include "vierkant/Semaphore.hpp"
#include "vierkant/Device.hpp"

namespace vierkant
{

using FencePtr = std::shared_ptr<VkFence_T>;

/**
 * @brief   Create a ref counted fence object
 *
 * @param   device      the VkDevice used to create the Fence
 * @param   signaled    flag indicating if the fence should be created in signaled state
 * @return  a newly created Fence
 */
FencePtr create_fence(const vierkant::DevicePtr &device, bool signaled = false);

/**
 * @brief   Wait for a fence to be signaled, optionally reset it.
 *
 * @param   fence   the fence to wait for.
 * @param   reset   flag indicating if the fence shall be reset to unsignaled after waiting for it.
 */
void wait_fence(const vierkant::DevicePtr &device, const vierkant::FencePtr &fence, bool reset = true);

/**
 * @brief   Submit an array of command-buffers and/or semaphores to a VkQueue.
 *
 * @param   device          shared handle to a VkDevice
 * @param   queue           a VkQueue to submit the commands to
 * @param   command_buffers an array of recorded VkCommandBuffers
 * @param   fence           an optional fence to signal
 * @param   wait_fence      optional flag indicating that the fence should be waited on
 * @param   semaphore_infos an optional array of semaphore_submit_info_t, can be used to pass in signal/wait semaphores
 */
void submit(const vierkant::DevicePtr &device,
            VkQueue queue,
            const std::vector<VkCommandBuffer> &command_buffers,
            bool wait_fence = false,
            VkFence fence = VK_NULL_HANDLE,
            const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos = {});

using CommandPoolPtr = std::shared_ptr<VkCommandPool_T>;

/**
 * @brief   Create a shared VkCommandPool.
 *
 * @param   device      shared handle to a VkDevice
 * @param   queue_type  type of queue (graphics|transfer|compute) this CommandPool is used for
 * @param   flags       flags to pass to VkCreateCommandPool
 * @return  the newly created shared VkCommandPool
 */
CommandPoolPtr create_command_pool(const vierkant::DevicePtr &device, vierkant::Device::Queue queue_type,
                                   VkCommandPoolCreateFlags flags);

class CommandBuffer
{
public:

    /**
     * @brief   construct a new CommandBuffer
     *
     * @param   device  the VkDevice that should be used to create the CommandBuffer
     * @param   command_pool    the VkCommandPool to allocate the CommandBuffer from
     * @param   level       the VkCommandBufferLevel
     * @param   the_queue   an optional VkQueue to automatically submit the CommandBuffer before destruction
     */
    CommandBuffer(DevicePtr device,
                  VkCommandPool command_pool,
                  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    CommandBuffer() = default;

    CommandBuffer(CommandBuffer &&other) noexcept;

    CommandBuffer(const CommandBuffer &) = delete;

    ~CommandBuffer();

    CommandBuffer &operator=(CommandBuffer other);

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
     * @param   wait_fence      flag indicating if synchronization (blocking wait) via internal fence should be done.
     * @param   fence           optional external VkFence object to wait on
     * @param   semaphore_infos an optional array of semaphore_submit_info_t, can be used to pass in signal/wait semaphores
     */
    void submit(VkQueue queue,
                bool wait_fence = false,
                VkFence fence = VK_NULL_HANDLE,
                const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos = {});

    /**
     * @brief   Reset the CommandBuffer back to an initial state,
     *          optionally freeing allocated resources
     *
     * @param   release_resources    flag indicating wether to free all allocated resources
     */
    void reset(bool release_resources = false);

    [[nodiscard]] VkCommandBuffer handle() const{ return m_handle; }

    [[nodiscard]] VkCommandPool pool() const{ return m_pool; }

    [[nodiscard]] bool is_recording() const{ return m_recording; }

    inline explicit operator bool() const{ return static_cast<bool>(m_handle); };

    friend void swap(CommandBuffer &lhs, CommandBuffer &rhs);

private:
    DevicePtr m_device = nullptr;
    VkCommandBuffer m_handle = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
    VkCommandPool m_pool = VK_NULL_HANDLE;
    bool m_recording = false;
};

}//namespace vulkan