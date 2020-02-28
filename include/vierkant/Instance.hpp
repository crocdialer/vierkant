//
// Created by crocdialer on 9/27/18.
//

#pragma once

#include <vulkan/vulkan.h>
#include "crocore/crocore.hpp"

namespace vierkant {

VkFormat find_depth_format(VkPhysicalDevice the_device);

/**
 * @brief   Helper function to check if the provided VkResult equals VK_SUCCESS.
 *          Otherwise throws a std::runtime_error with provided fail_msg
 * @param   res
 * @param   fail_msg
 */
void vkCheck(VkResult res, const std::string &fail_msg);

/**
 * Instance encapsulates a VkInstance.
 * It provides initialization, access to physical devices
 * and debugging resources.
 * Instance is default- and move- but NOT copy-constructable
 */
class Instance
{
public:

    /**
     * @brief the vulkan-api version used
     */
    static constexpr int apiVersion = VK_API_VERSION_1_1;

    /**
     * @brief   construct an initialized vulkan instance
     * @param   use_validation_layers       use validation layers (VK_LAYER_LUNARG_standard_validation) or not
     * @param   the_required_extensions     a list of required extensions (e.g. VK_KHR_SWAPCHAIN_EXTENSION_NAME)
     */
    Instance(bool use_validation_layers, const std::vector<const char *> &the_required_extensions);

    Instance() = default;

    Instance(const Instance &) = delete;

    Instance(Instance &&other) noexcept;

    ~Instance();

    Instance &operator=(Instance other);

    /**
     * @return  true if validation layers are in use, false otherwise
     */
    bool use_validation_layers() const { return m_debug_callback; }

    /**
     * @return  a handle to the managed VKInstance
     */
    VkInstance handle() const { return m_handle; }

    /**
     * @return  an array of all available physical GPU-devices
     */
    const std::vector<VkPhysicalDevice> &physical_devices() const { return m_physical_devices; }

    const std::vector<const char *> &extensions() const { return m_extensions; }

    inline explicit operator bool() const { return static_cast<bool>(m_handle); };

    friend void swap(Instance &lhs, Instance &rhs);

private:

    /**
     * @brief   initialize the vulkan instance
     * @param   use_validation_layers       use validation layers (VK_LAYER_LUNARG_standard_validation) or not
     * @param   the_required_extensions     a list of required extensions (e.g. VK_KHR_SWAPCHAIN_EXTENSION_NAME)
     */
    bool init(bool use_validation_layers, const std::vector<const char *> &the_required_extensions);

    void setup_debug_callback();

    std::vector<const char *> m_extensions;

    // vulkan instance
    VkInstance m_handle = VK_NULL_HANDLE;

    // physical devices
    std::vector<VkPhysicalDevice> m_physical_devices;

    // debug callback
    VkDebugReportCallbackEXT m_debug_callback = VK_NULL_HANDLE;
};

}//namespace vulkan