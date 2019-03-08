//
// Created by crocdialer on 2/8/19.
//

#pragma once

#include "Instance.hpp"
#include "vk_mem_alloc.h"

namespace vierkant {

DEFINE_CLASS_PTR(Device);

class Device
{
public:

    static DevicePtr create(VkPhysicalDevice pd,
                            bool use_validation = false,
                            VkSurfaceKHR surface = VK_NULL_HANDLE,
                            VkPhysicalDeviceFeatures device_features = {});

    Device(const Device &) = delete;

    Device(Device &&) = delete;

    Device &operator=(Device other) = delete;

    ~Device();

    struct QueueFamilyIndices
    {
        int graphics_family = -1;
        int transfer_family = -1;
        int compute_family = -1;
        int present_family = -1;
    };

    /**
     * @return  the managed VkDevice
     */
    VkDevice handle() const { return m_device; }

    /**
     * @return  the associated VkPhysicalDevice
     */
    VkPhysicalDevice physical_device() const { return m_physical_device; }

    /**
     * @return  handle for graphics queue
     */
    VkQueue graphics_queue() const { return m_graphics_queue; }

    /**
     * @return  handle for transfer queue
     */
    VkQueue transfer_queue() const { return m_transfer_queue; }

    /**
     * @return  handle for compute queue
     */
    VkQueue compute_queue() const { return m_compute_queue; }

    /**
     * @return  handle for presentation queue
     */
    VkQueue present_queue() const { return m_present_queue; }

    /**
     * @return  const ref to the used QueueFamilyIndices
     */
    const QueueFamilyIndices &queue_family_indices() const { return m_queue_family_indices; }

    /**
     * @return  handle for command pool
     */
    VkCommandPool command_pool() const { return m_command_pool; }

    /**
     * @return  handle for transient command pool
     */
    VkCommandPool command_pool_transient() const { return m_command_pool_transient; }

    /**
     * @return  handle for transient command pool
     */
    VkCommandPool command_pool_transfer() const { return m_command_pool_transfer; }

    /**
     * @return  enum stating the maximum available number of samples for MSAA
     */
    VkSampleCountFlagBits max_usable_samples() const { return m_max_usable_samples; }

    /**
     * @return  handle for memory allocator
     */
    VmaAllocator vk_mem_allocator() const { return m_vk_mem_allocator; };

private:

    Device(VkPhysicalDevice pd, bool use_validation, VkSurfaceKHR surface,
           VkPhysicalDeviceFeatures device_features);

    // physical device
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

    // logical device
    VkDevice m_device = VK_NULL_HANDLE;

    // an instance of a VmaAllocator for this device
    VmaAllocator m_vk_mem_allocator = VK_NULL_HANDLE;

    VkSampleCountFlagBits m_max_usable_samples = VK_SAMPLE_COUNT_1_BIT;

    // graphics queue for logical device
    VkQueue m_graphics_queue = VK_NULL_HANDLE;

    // transfer queue for logical device
    VkQueue m_transfer_queue = VK_NULL_HANDLE;

    // compute queue for logical device
    VkQueue m_compute_queue = VK_NULL_HANDLE;

    // presentation queue for logical device
    VkQueue m_present_queue = VK_NULL_HANDLE;

    // keeps track of queue family indices
    QueueFamilyIndices m_queue_family_indices;

    // regular command pool (graphics queue)
    VkCommandPool m_command_pool = VK_NULL_HANDLE;

    // transient command pool (graphics queue)
    VkCommandPool m_command_pool_transient = VK_NULL_HANDLE;

    // transient command pool (transfer queue)
    VkCommandPool m_command_pool_transfer = VK_NULL_HANDLE;
};

}//namespace vulkan