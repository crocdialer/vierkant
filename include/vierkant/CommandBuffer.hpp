//
// Created by crocdialer on 9/29/18.
//

#pragma once

#include "vierkant/Device.hpp"

namespace vierkant {

using CommandPoolPtr = std::shared_ptr<VkCommandPool_T>;

class CommandBuffer
{
public:

    /**
     * @brief   construct a new CommandBuffer
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
     * @param   flags
     */
    void begin(VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
               VkCommandBufferInheritanceInfo *inheritance = nullptr);

    /**
     * @brief   stop recording commands into this buffer
     */
    void end();

    /**
     *
     * @param   queue       VkQueue to submit this CommandBuffer to
     * @param   create_fence
     * @param   fence       optional external VkFence object to wait on
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