//
// Created by crocdialer on 2/8/19.
//

#pragma once

#include <map>
#include <vierkant/Instance.hpp>
#include <vierkant/debug_label.hpp>
#include <vierkant/math.hpp>
#include <vk_mem_alloc.h>

namespace vierkant
{

DEFINE_CLASS_PTR(Device)

//! define a shared handle for a VkQueryPool
using QueryPoolPtr = std::shared_ptr<VkQueryPool_T>;

//! define a shared handle for a VkSampler
using VkSamplerPtr = std::shared_ptr<VkSampler_T>;

/**
 * @brief   sampler_state_t describes the sampling-state a VkSampler is created from.
 *          it is default-constructable, comparable and hashable, used as key by vierkant::Device::sampler.
 *
 *          note: the mip-range is deliberately not part of the state. a sampler's maxLod is set to
 *          VK_LOD_CLAMP_NONE, because the sampled mip-level is already clamped to the image-view's
 *          level-range. keeping a per-image maxLod would only fragment the cache.
 */
struct sampler_state_t
{
    VkFilter min_filter = VK_FILTER_LINEAR;
    VkFilter mag_filter = VK_FILTER_LINEAR;
    VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerReductionMode reduction_mode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    float max_anisotropy = 0.f;
    bool normalized_coords = true;

    bool operator==(const sampler_state_t &other) const = default;
};

}// namespace vierkant

namespace std
{
template<>
struct hash<vierkant::sampler_state_t>
{
    size_t operator()(const vierkant::sampler_state_t &state) const;
};
}// namespace std

namespace vierkant
{

QueryPoolPtr create_query_pool(const vierkant::DevicePtr &device, uint32_t query_count, VkQueryType query_type);

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

    struct queue_asset_t
    {
        VkQueue queue = VK_NULL_HANDLE;
        std::unique_ptr<std::recursive_mutex> mutex;
    };

    struct properties_t
    {
        VkPhysicalDeviceProperties core;
        VkPhysicalDeviceVulkan12Properties vulkan12;
        VkPhysicalDeviceVulkan13Properties vulkan13;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_pipeline;
        VkPhysicalDeviceOpacityMicromapPropertiesEXT micromap_opacity;
        VkPhysicalDeviceMeshShaderPropertiesEXT mesh_shader;
    };

    //! memory-usage for a single device memory-heap
    struct memory_budget_t
    {
        //! bytes of VkDeviceMemory allocated from this heap
        VkDeviceSize block_bytes = 0;

        //! bytes actually in use by allocations. below 'block_bytes' by the amount kept as pool-headroom
        VkDeviceSize allocation_bytes = 0;

        //! estimated current usage and available budget, as reported by VMA
        VkDeviceSize usage = 0, budget = 0;
    };

    struct create_info_t
    {
        //! handle for the vulkan-instance
        VkInstance instance = VK_NULL_HANDLE;

        //! the physical device to use
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;

        //! enable validation layers
        bool use_validation = false;

        //! short-circuit function-pointers directly to device/driver entries (useful if only a single device exists)
        bool direct_function_pointers = false;

        //! maximum number of queues to create, default: 0 (no limit)
        uint32_t max_num_queues = 0;

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
    void wait_idle() const;

    /**
     * @return a struct grouping physical-device properties
     */
    [[nodiscard]] const properties_t &properties() const { return m_properties; };

    /**
     * @brief   query current memory-usage per device memory-heap. cheap enough to call every frame.
     *
     * @return  an array of memory_budget_t, one per memory-heap.
     */
    [[nodiscard]] std::vector<memory_budget_t> memory_budgets() const;

    /**
     * @return  handle for the highest-priority-queue of a certain type
     *          or VK_NULL_HANDLE if not present.
     */
    [[nodiscard]] VkQueue queue(Queue type = Queue::GRAPHICS) const;

    /**
     * @return  handle for queues
     */
    [[nodiscard]] const std::vector<queue_asset_t> &queues(Queue type) const;

    /**
     * @return  nullptr or a pointer to an asset-struct for a queue
     */
    [[nodiscard]] const queue_asset_t *queue_asset(VkQueue queue) const;

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
    void set_object_name(uint64_t handle, VkObjectType type, const std::string &name) const;

    /**
     * @brief   sampler returns a shared VkSampler for a provided sampler-state.
     *          samplers are cached and shared device-wide, distinct sampler-states are rare.
     *
     * @param   state   a provided sampler_state_t
     * @return  a retrieved or newly created, shared VkSampler
     */
    VkSamplerPtr sampler(const sampler_state_t &state);

private:
    explicit Device(const create_info_t &create_info);

    // physical device
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

    // group physical device properties
    properties_t m_properties = {};

    // logical device
    VkDevice m_device = VK_NULL_HANDLE;

    // an instance of a VmaAllocator for this device
    VmaAllocator m_vk_mem_allocator = VK_NULL_HANDLE;

    VkSampleCountFlagBits m_max_usable_samples = VK_SAMPLE_COUNT_1_BIT;

    // a map holding all queues for logical device
    std::map<Queue, std::vector<queue_asset_t>> m_queues;
    std::unordered_map<VkQueue, const queue_asset_t *> m_queue_map;

    // keeps track of queue family indices
    std::map<Queue, queue_family_info_t> m_queue_indices;

    // transient command pool (graphics queue)
    VkCommandPool m_command_pool_transient = VK_NULL_HANDLE;

    // transient command pool (transfer queue)
    VkCommandPool m_command_pool_transfer = VK_NULL_HANDLE;

    // device-wide cache of shared samplers (assets can be loaded from worker-threads)
    std::unordered_map<sampler_state_t, VkSamplerPtr> m_samplers;
    std::mutex m_sampler_mutex;
};

}// namespace vierkant