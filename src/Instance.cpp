//
// Created by crocdialer on 9/27/18.
//

#define VK_NO_PROTOTYPES
#include <volk.h>

#include <iostream>
#include <set>

#include "vierkant/Instance.hpp"

namespace vierkant
{

////////////////////////////// VALIDATION LAYER ///////////////////////////////////////////////////

const std::vector<const char *> g_validation_layers = {"VK_LAYER_KHRONOS_validation"};

bool check_validation_layer_support()
{
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for(const char *layerName: g_validation_layers)
    {
        bool layer_found = false;

        for(const auto &layerProperties: available_layers)
        {
            if(strcmp(layerName, layerProperties.layerName) == 0)
            {
                layer_found = true;
                break;
            }
        }
        if(!layer_found) { return false; }
    }
    return true;
}

VkResult CreateDebugUtilMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pCallback)
{
    if(vkCreateDebugUtilsMessengerEXT)
    {
        return vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pCallback);
    }
    else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
}

void DestroyDebugUtilMessenger(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                               const VkAllocationCallbacks *pAllocator)
{
    if(vkDestroyDebugUtilsMessengerEXT) { vkDestroyDebugUtilsMessengerEXT(instance, callback, pAllocator); }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT type_flags,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *data, void *user_data)
{
    if(user_data && data)
    {
        Instance::debug_fn_t &debug_fn = *reinterpret_cast<Instance::debug_fn_t *>(user_data);
        if(debug_fn) { debug_fn(msg_severity, type_flags, data); }
    }
    else if(data) { spdlog::error(data->pMessage); }
    return VK_FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Instance::setup_debug_callback()
{
    if(m_debug_messenger)
    {
        DestroyDebugUtilMessenger(m_handle, m_debug_messenger, nullptr);
        m_debug_messenger = nullptr;
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = {};
    debug_utils_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_utils_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_utils_create_info.pfnUserCallback = debug_callback;
    debug_utils_create_info.pUserData = m_debug_fn ? &m_debug_fn : nullptr;

    vkCheck(CreateDebugUtilMessenger(m_handle, &debug_utils_create_info, nullptr, &m_debug_messenger),
            "failed to set up debug callback!");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool check_device_extension_support(VkPhysicalDevice device, const std::vector<const char *> &extensions)
{
    uint32_t num_extensions;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, nullptr);
    std::vector<VkExtensionProperties> extensions_available(num_extensions);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, extensions_available.data());
    std::set<std::string> extensions_required(extensions.begin(), extensions.end());
    for(const auto &extension: extensions_available) { extensions_required.erase(extension.extensionName); }
    return extensions_required.empty();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void vkCheck(VkResult res, const std::string &fail_msg)
{
    if(res != VK_SUCCESS) { throw std::runtime_error(fail_msg); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkFormat find_supported_format(VkPhysicalDevice the_device, const std::vector<VkFormat> &the_candidates,
                               VkImageTiling the_tiling, VkFormatFeatureFlags the_features)
{
    for(VkFormat format: the_candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(the_device, format, &props);

        if(the_tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & the_features) == the_features)
        {
            return format;
        }
        else if(the_tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & the_features) == the_features)
        {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkFormat find_depth_format(VkPhysicalDevice device)
{
    return find_supported_format(device,
                                 {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
                                 VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Instance::Instance(const create_info_t &create_info)
{
    if(!init(create_info)) { *this = Instance(); }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Instance::Instance(Instance &&other) noexcept : Instance() { swap(*this, other); }

///////////////////////////////////////////////////////////////////////////////////////////////////

Instance::~Instance()
{
    if(m_debug_messenger)
    {
        DestroyDebugUtilMessenger(m_handle, m_debug_messenger, nullptr);
        m_debug_messenger = nullptr;
    }
    if(m_handle)
    {
        vkDestroyInstance(m_handle, nullptr);
        m_handle = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Instance &Instance::operator=(Instance other)
{
    swap(*this, other);

    // set user-pointer to current instance
    if(m_debug_fn) { set_debug_fn(m_debug_fn); }
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Instance::init(const create_info_t &create_info)
{
    vkCheck(volkInitialize(), "failed during 'volkInitialize()': unable to retrieve vulkan function-pointers");

    auto used_extensions = create_info.extensions;
    if(create_info.use_validation_layers || create_info.use_debug_labels)
    {
        used_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // check support for validation-layers
    if(create_info.use_validation_layers && !check_validation_layer_support())
    {
        spdlog::error("validation layers requested, but not available!");
        return false;
    }

    // query extension support
    uint32_t num_extensions = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, nullptr);

    // allocate memory and fill
    std::vector<VkExtensionProperties> extensions(num_extensions);
    vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, extensions.data());

    spdlog::trace("available extensions: ");
    for(const auto &ext: extensions) { spdlog::trace(ext.extensionName); }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Generic Vierkant Application";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Vierkant";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = api_version;

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;

    // extensions
    instance_create_info.enabledExtensionCount = used_extensions.size();
    instance_create_info.ppEnabledExtensionNames = used_extensions.data();
    instance_create_info.enabledLayerCount =
            create_info.use_validation_layers ? static_cast<uint32_t>(g_validation_layers.size()) : 0;
    instance_create_info.ppEnabledLayerNames = create_info.use_validation_layers ? g_validation_layers.data() : nullptr;

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = {};
    debug_utils_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_utils_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_utils_create_info.pfnUserCallback = debug_callback;

    // request debug_utils only if validation was requested
    instance_create_info.pNext = create_info.use_validation_layers ? &debug_utils_create_info : nullptr;
    
    // portability flag (e.g. required for Molten VK on)
    instance_create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    // create the vulkan instance
    vkCheck(vkCreateInstance(&instance_create_info, nullptr, &m_handle), "failed to create instance!");

    // load all instance-functions pointers dynamically
    volkLoadInstance(m_handle);

    if(create_info.use_validation_layers)
    {
        vkCheck(CreateDebugUtilMessenger(m_handle, &debug_utils_create_info, nullptr, &m_debug_messenger),
                "failed to create 'VkDebugUtilsMessengerEXT'");
    }
    m_extensions = used_extensions;
    uint32_t num_devices = 0;
    vkEnumeratePhysicalDevices(m_handle, &num_devices, nullptr);

    if(!num_devices) { return false; }

    m_physical_devices.resize(num_devices);
    vkEnumeratePhysicalDevices(m_handle, &num_devices, m_physical_devices.data());

    // attach logger for debug-output
    set_debug_fn([](VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity, VkDebugUtilsMessageTypeFlagsEXT type_flags,
                    const VkDebugUtilsMessengerCallbackDataEXT *data) {
        if(type_flags & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ||
           type_flags & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        {
            if(msg_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) { spdlog::error(data->pMessage); }
            else if(msg_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) { spdlog::warn(data->pMessage); }
            else if(msg_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) { spdlog::debug(data->pMessage); }
        }
    });

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Instance::set_debug_fn(Instance::debug_fn_t debug_fn)
{
    if(use_validation_layers())
    {
        m_debug_fn = std::move(debug_fn);
        setup_debug_callback();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Instance &lhs, Instance &rhs)
{
    std::swap(lhs.m_extensions, rhs.m_extensions);
    std::swap(lhs.m_handle, rhs.m_handle);
    std::swap(lhs.m_physical_devices, rhs.m_physical_devices);
    std::swap(lhs.m_debug_messenger, rhs.m_debug_messenger);
    std::swap(lhs.m_debug_fn, rhs.m_debug_fn);
}

}// namespace vierkant
