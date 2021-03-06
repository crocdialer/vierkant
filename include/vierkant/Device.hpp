//
// Created by crocdialer on 2/8/19.
//

#pragma once

#include <map>
#include <vierkant/vk_mem_alloc.h>
#include <vierkant/Instance.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Device);

using VmaPoolPtr = std::shared_ptr<VmaPool_T>;

class Device
{
public:

    struct create_info_t
    {
        //! handle for the vulkan-instance
        VkInstance instance = VK_NULL_HANDLE;

        //! the physical device to use
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;

        //! enable validation layers
        bool use_validation = false;

        //! enable the VkDeviceAddress feature
        bool enable_device_address = false;

        //! optional VkSurface
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        VkPhysicalDeviceFeatures device_features = {};

        std::vector<const char*> extensions;

        //! optional pointer that will be passed as 'pNext' during device-creation.
        void* create_device_pNext = nullptr;
    };

    static DevicePtr create(const create_info_t &create_info);

    Device(const Device &) = delete;

    Device(Device &&) = delete;

    Device &operator=(Device other) = delete;

    ~Device();

    enum class Queue
    {
        GRAPHICS, TRANSFER, COMPUTE, PRESENT
    };

    struct queue_family_info_t
    {
        int index = -1;
        uint32_t num_queues = 0;
    };

    /**
     * @return  the managed VkDevice
     */
    VkDevice handle() const{ return m_device; }

    /**
     * @return  the associated VkPhysicalDevice
     */
    VkPhysicalDevice physical_device() const{ return m_physical_device; }

    /**
     * @return the physical device properties
     */
    const VkPhysicalDeviceProperties &properties() const{ return m_physical_device_properties.properties; };

    /**
     * @return  handle for the highest-priority-queue of a certain type
     *          or VK_NULL_HANDLE if not present.
     */
    VkQueue queue(Queue type = Queue::GRAPHICS) const;

    /**
     * @return  handle for queues
     */
    const std::vector<VkQueue> &queues(Queue type) const;

    /**
     * @return  const ref to the used QueueFamilyIndices
     */
    const std::map<Queue, queue_family_info_t> &queue_family_indices() const{ return m_queue_indices; }

    /**
     * @return  handle for command pool
     */
    VkCommandPool command_pool() const{ return m_command_pool; }

    /**
     * @return  handle for transient command pool
     */
    VkCommandPool command_pool_transient() const{ return m_command_pool_transient; }

    /**
     * @return  handle for transient command pool
     */
    VkCommandPool command_pool_transfer() const{ return m_command_pool_transfer; }

    /**
     * @return  enum stating the maximum available number of samples for MSAA
     */
    VkSampleCountFlagBits max_usable_samples() const{ return m_max_usable_samples; }

    /**
     * @return  handle for memory allocator
     */
    VmaAllocator vk_mem_allocator() const{ return m_vk_mem_allocator; };

private:

    explicit Device(const create_info_t &create_info);

    // physical device
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

    // physical device properties
    VkPhysicalDeviceProperties2 m_physical_device_properties = {};

    // logical device
    VkDevice m_device = VK_NULL_HANDLE;

    // an instance of a VmaAllocator for this device
    VmaAllocator m_vk_mem_allocator = VK_NULL_HANDLE;

    VkSampleCountFlagBits m_max_usable_samples = VK_SAMPLE_COUNT_1_BIT;

    // a map holding all queues for logical device
    std::map<Queue, std::vector<VkQueue>> m_queues;

    // keeps track of queue family indices
    std::map<Queue, queue_family_info_t> m_queue_indices;

    // regular command pool (graphics queue)
    VkCommandPool m_command_pool = VK_NULL_HANDLE;

    // transient command pool (graphics queue)
    VkCommandPool m_command_pool_transient = VK_NULL_HANDLE;

    // transient command pool (transfer queue)
    VkCommandPool m_command_pool_transfer = VK_NULL_HANDLE;
};

}//namespace vulkan