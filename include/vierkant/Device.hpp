//
// Created by crocdialer on 2/8/19.
//

#pragma once

#include <map>
#include <vierkant/Instance.hpp>
#include <vierkant/math.hpp>
#include <vierkant/debug_label.hpp>
#include <vk_mem_alloc.h>

namespace vierkant
{

DEFINE_CLASS_PTR(Device)

//! define a shared handle for a VkQueryPool
using QueryPoolPtr = std::shared_ptr<VkQueryPool_T>;

QueryPoolPtr create_query_pool(const vierkant::DevicePtr &device, uint32_t query_count, VkQueryType query_type);

double timestamp_millis(const uint64_t *timestamps, int32_t idx, float timestamp_period);

/**
 * @brief   device_info can be used to retrieve a descriptive string about a physical device,
 *          including information about used vulkan and vierkant-versions
 *
 * @param   physical_device         provided handle to a VkPhysicalDevice.
 * @return  a descriptive string
 */
std::string device_info(VkPhysicalDevice physical_device);

VkPhysicalDeviceProperties2 device_properties(VkPhysicalDevice physical_device);

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

        //! use debug_utils extension
        bool debug_labels = false;

        //! short-circuit function-pointers directly to device/driver entries (useful if only a single device exists)
        bool direct_function_pointers = false;

        //! optional VkSurface
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        VkPhysicalDeviceFeatures device_features = {};

        std::vector<const char *> extensions;

        //! optional pointer that will be passed as 'pNext' during device-creation.
        void *create_device_pNext = nullptr;
    };

    static DevicePtr create(const create_info_t &create_info);

    Device(const Device &) = delete;

    Device(Device &&) = delete;

    Device &operator=(Device other) = delete;

    ~Device();

    enum class Queue
    {
        GRAPHICS,
        TRANSFER,
        COMPUTE,
        PRESENT
    };

    struct queue_family_info_t
    {
        int index = -1;
        uint32_t num_queues = 0;
    };

    /**
     * @return  the managed VkDevice
     */
    [[nodiscard]] VkDevice handle() const { return m_device; }

    /**
     * @return  the associated VkPhysicalDevice
     */
    [[nodiscard]] VkPhysicalDevice physical_device() const { return m_physical_device; }

    /**
     * @brief   wait for the device to become idle
     */
    void wait_idle();

    /**
     * @return the physical device properties
     */
    [[nodiscard]] const VkPhysicalDeviceProperties &properties() const
    {
        return m_physical_device_properties.properties;
    };

    /**
     * @return  handle for the highest-priority-queue of a certain type
     *          or VK_NULL_HANDLE if not present.
     */
    [[nodiscard]] VkQueue queue(Queue type = Queue::GRAPHICS) const;

    /**
     * @return  handle for queues
     */
    [[nodiscard]] const std::vector<VkQueue> &queues(Queue type) const;

    /**
     * @return  const ref to the used QueueFamilyIndices
     */
    [[nodiscard]] const std::map<Queue, queue_family_info_t> &queue_family_indices() const { return m_queue_indices; }

    /**
     * @return  handle for transient command pool
     */
    [[nodiscard]] VkCommandPool command_pool_transient() const { return m_command_pool_transient; }

    /**
     * @return  handle for transient command pool
     */
    [[nodiscard]] VkCommandPool command_pool_transfer() const { return m_command_pool_transfer; }

    /**
     * @return  enum stating the maximum available number of samples for MSAA
     */
    [[nodiscard]] VkSampleCountFlagBits max_usable_samples() const { return m_max_usable_samples; }

    /**
     * @return  handle for memory allocator
     */
    [[nodiscard]] VmaAllocator vk_mem_allocator() const { return m_vk_mem_allocator; };

    /**
     * @brief   set_object_name can be used to set a name for an object.
     *
     * @param   handle  an arbitrary vulkan-handle
     * @param   type    an object-type identifier
     * @param   name    a name to use for this object
     */
    void set_object_name(VkDeviceAddress handle, VkObjectType type, const std::string &name);

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

    // transient command pool (graphics queue)
    VkCommandPool m_command_pool_transient = VK_NULL_HANDLE;

    // transient command pool (transfer queue)
    VkCommandPool m_command_pool_transfer = VK_NULL_HANDLE;
};

}// namespace vierkant