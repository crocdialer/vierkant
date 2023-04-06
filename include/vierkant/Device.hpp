//
// Created by crocdialer on 2/8/19.
//

#pragma once

#include <map>
#include <vierkant/Instance.hpp>
#include <vierkant/math.hpp>
#include <vk_mem_alloc.h>

namespace vierkant
{

DEFINE_CLASS_PTR(Device);

//! define a shared handle for a VkQueryPool
using QueryPoolPtr = std::shared_ptr<VkQueryPool_T>;

QueryPoolPtr create_query_pool(const vierkant::DevicePtr &device, uint32_t query_count, VkQueryType query_type);

double timestamp_millis(const uint64_t *timestamps, int32_t idx, float timestamp_period);

using VmaPoolPtr = std::shared_ptr<VmaPool_T>;

class Device
{
public:

    struct debug_label_t
    {
        std::string text;
        glm::vec4 color = {0.6f, 0.6f, 0.6f, 1.f};
    };

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

        //! enable raytracing device-features
        bool use_raytracing = false;

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
     * @brief   'begin_label' can be used to mark the start of a labeled section within a commandbuffer.
     *
     * @param   commandbuffer   a provided commandbuffer
     * @param   label           a debug-label object.
     */
    void begin_label(VkCommandBuffer commandbuffer, const debug_label_t &label);

    /**
     * @brief   'begin_label' can be used to mark the start of a labeled section within a queue.
     *
     * @param   queue   a provided queue
     * @param   label   a debug-label object.
     */
    void begin_label(VkQueue queue, const debug_label_t &label);

    /**
     * @brief   'end_label' needs to be used after previous calls to 'begin_label',
     *          to mark the end of a labeled section within a commandbuffer.
     *
     * @param   commandbuffer   a provided commandbuffer.
     */
    void end_label(VkCommandBuffer commandbuffer);

    /**
     * @brief   'end_label' needs to be used after previous calls to 'begin_label',
     *          to mark the end of a labeled section within a queue.
     *
     * @param   queue   a provided queue.
     */
    void end_label(VkQueue queue);

    /**
     * @brief   insert_label can be used to insert a singular label into a commandbuffer.
     *
     * @param   commandbuffer   a provided commandbuffer
     * @param   label           a debug-label object.
     */
    void insert_label(VkCommandBuffer commandbuffer, const debug_label_t &label);

    /**
     * @brief   insert_label can be used to insert a singular label into a queue.
     *
     * @param   queue   a provided queue
     * @param   label   a debug-label object.
     */
    void insert_label(VkQueue queue, const debug_label_t &label);

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

    //! debug-labels (VK_EXT_debug_utils)
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT = nullptr;
    PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT = nullptr;
    PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT = nullptr;
};

}// namespace vierkant