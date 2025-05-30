//
// Created by crocdialer on 9/27/18.
//

#pragma once

#include <crocore/crocore.hpp>
#include <volk.h>

namespace vierkant
{

/**
 * @brief   find a matching depth-format for a provided device.
 *
 * @param   device  a provided device.
 * @return  a matching depth-format
 */
VkFormat find_depth_format(VkPhysicalDevice device);

/**
 * @brief   check_instance_extension_support can be used to check if a list of instance-extensions is supported.
 *
 * @param   extensions  a provided list of extension-strings
 * @return  true, if all provided extensions are supported
 */
[[maybe_unused]] bool check_instance_extension_support(const std::vector<const char *> &extensions);

/**
 * @brief   check_device_extension_support can be used to check if a list of device-extensions is supported.
 *
 * @param   device      a provided device.
 * @param   extensions  a provided list of extension-strings
 * @return  true, if all provided extensions are supported
 */
[[maybe_unused]] bool check_device_extension_support(VkPhysicalDevice device,
                                                     const std::vector<const char *> &extensions);

/**
 * @brief   Helper function to check if the provided VkResult equals VK_SUCCESS.
 *          Otherwise throws a std::runtime_error with provided fail_msg
 * @param   res
 * @param   fail_msg
 */
void vkCheck(VkResult res, const std::string &fail_msg);

/**
 * @brief   Helper function to return an aligned size
 *
 * @param   size        a size
 * @param   alignment   required alignment
 */
static inline uint32_t aligned_size(uint32_t size, uint32_t alignment)
{
    assert(crocore::is_pow_2(alignment));
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * Instance encapsulates a VkInstance.
 * It provides initialization, access to physical devices
 * and debugging resources.
 * Instance is default- and move- but NOT copy-constructable
 */
class Instance
{
public:
    using debug_fn_t = std::function<void(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                          const VkDebugUtilsMessengerCallbackDataEXT *)>;

    /**
     * @brief the vulkan-api version used
     */
    static constexpr int api_version = VK_API_VERSION_1_3;

    struct create_info_t
    {
        bool use_validation_layers = false;
        bool use_debug_labels = false;
        std::vector<const char *> extensions;
    };

    /**
     * @brief   construct an initialized vulkan instance
     * @param   use_validation_layers       use validation layers (VK_LAYER_LUNARG_standard_validation) or not
     * @param   the_required_extensions     a list of required extensions (e.g. VK_KHR_SWAPCHAIN_EXTENSION_NAME)
     */
    explicit Instance(const create_info_t &create_info);

    Instance() = default;

    Instance(const Instance &) = delete;

    Instance(Instance &&other) noexcept;

    ~Instance();

    Instance &operator=(Instance other);

    /**
     * @return  true if validation layers are in use, false otherwise
     */
    [[nodiscard]] bool use_validation_layers() const { return m_debug_messenger; }

    /**
     * @brief   set a debug-callback, containing output from validation-layers.
     *
     * @param   debug_fn    a function-object used as debug-callback.
     */
    void set_debug_fn(debug_fn_t debug_fn);

    /**
     * @return  a handle to the managed VKInstance
     */
    [[nodiscard]] VkInstance handle() const { return m_handle; }

    /**
     * @return  an array of all available physical GPU-devices
     */
    [[nodiscard]] const std::vector<VkPhysicalDevice> &physical_devices() const { return m_physical_devices; }

    [[nodiscard]] const std::vector<const char *> &extensions() const { return m_extensions; }

    inline explicit operator bool() const { return static_cast<bool>(m_handle); };

    friend void swap(Instance &lhs, Instance &rhs);

private:
    /**
     * @brief   initialize the vulkan instance
     * @param   use_validation_layers       use validation layers (VK_LAYER_LUNARG_standard_validation) or not
     * @param   the_required_extensions     a list of required extensions (e.g. VK_KHR_SWAPCHAIN_EXTENSION_NAME)
     */
    bool init(const create_info_t &create_info);

    void setup_debug_callback();

    std::vector<const char *> m_extensions;

    // vulkan instance
    VkInstance m_handle = VK_NULL_HANDLE;

    // physical devices
    std::vector<VkPhysicalDevice> m_physical_devices;

    // debug callback
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;

    // optional debug-function
    debug_fn_t m_debug_fn;
};

}// namespace vierkant