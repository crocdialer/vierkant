//
// Created by crocdialer on 9/27/18.
//

#pragma once

#include <vulkan/vulkan.h>

#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

//! forward declare a class and define shared-, const-, weak- and unique-pointers for it.
#define DEFINE_CLASS_PTR(CLASS_NAME)\
class CLASS_NAME;\
using CLASS_NAME##Ptr = std::shared_ptr<CLASS_NAME>;\
using CLASS_NAME##ConstPtr = std::shared_ptr<const CLASS_NAME>;\
using CLASS_NAME##WeakPtr = std::weak_ptr<CLASS_NAME>;\
using CLASS_NAME##UPtr = std::unique_ptr<CLASS_NAME>;


// define main-namespace and an alias for it
namespace vierkant{}
namespace vk = vierkant;

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