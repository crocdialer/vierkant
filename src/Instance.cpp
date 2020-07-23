//
// Created by crocdialer on 9/27/18.
//

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

    for(const char *layerName : g_validation_layers)
    {
        bool layer_found = false;

        for(const auto &layerProperties : available_layers)
        {
            if(strcmp(layerName, layerProperties.layerName) == 0)
            {
                layer_found = true;
                break;
            }
        }
        if(!layer_found){ return false; }
    }
    return true;
}

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback)
{
    auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if(func){ return func(instance, pCreateInfo, pAllocator, pCallback); }
    else{ return VK_ERROR_EXTENSION_NOT_PRESENT; }
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback,
                                   const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance,
                                                                            "vkDestroyDebugReportCallbackEXT");
    if(func){ func(instance, callback, pAllocator); }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags,
                                                     VkDebugReportObjectTypeEXT obj_type,
                                                     uint64_t obj,
                                                     size_t location,
                                                     int32_t code,
                                                     const char *layer_prefix,
                                                     const char *msg,
                                                     void *user_data)
{
    Instance::debug_fn_t &debug_fn = *reinterpret_cast<Instance::debug_fn_t *>(user_data);
    if(debug_fn){ debug_fn(msg); }
    else{ std::cerr << "validation layer: " << msg << std::endl; }
    return VK_FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const std::vector<const char *> g_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

//! check if all required extensions are available
bool check_device_extension_support(VkPhysicalDevice device)
{
    uint32_t num_extensions;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, nullptr);
    std::vector<VkExtensionProperties> extensions_available(num_extensions);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, extensions_available.data());
    std::set<std::string> extensions_required(g_device_extensions.begin(), g_device_extensions.end());
    for(const auto &extension : extensions_available){ extensions_required.erase(extension.extensionName); }
    return extensions_required.empty();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void vkCheck(VkResult res, const std::string &fail_msg)
{
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error(fail_msg);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VkFormat find_supported_format(VkPhysicalDevice the_device,
                               const std::vector<VkFormat> &the_candidates,
                               VkImageTiling the_tiling,
                               VkFormatFeatureFlags the_features)
{
    for(VkFormat format : the_candidates)
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

VkFormat find_depth_format(VkPhysicalDevice the_device)
{
    return find_supported_format(the_device,
                                 {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Instance::Instance(bool use_validation_layers, const std::vector<const char *> &the_required_extensions)
{
    if(!init(use_validation_layers, the_required_extensions))
    {
        *this = Instance();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Instance::Instance(Instance &&other) noexcept:
        Instance()
{
    swap(*this, other);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Instance::~Instance()
{
    if(m_debug_callback)
    {
        DestroyDebugReportCallbackEXT(m_handle, m_debug_callback, nullptr);
        m_debug_callback = nullptr;
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
    return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Instance::init(bool use_validation_layers, const std::vector<const char *> &the_required_extensions)
{
    auto required_extensions = the_required_extensions;
    if(use_validation_layers){ required_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME); }

    // check support for validation-layers
    if(use_validation_layers && !check_validation_layer_support())
    {
        std::cerr << "validation layers requested, but not available!";
        return false;
    }

    // query extension support
    uint32_t num_extensions = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, nullptr);

    // allocate memory and fill
    std::vector<VkExtensionProperties> extensions(num_extensions);
    vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, extensions.data());

    LOG_TRACE << "available extensions:";
    for(const auto &ext : extensions){ LOG_TRACE << "\t" << ext.extensionName; }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Generic Vierkant Application";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Vierkant";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // extensions
    create_info.enabledExtensionCount = required_extensions.size();
    create_info.ppEnabledExtensionNames = required_extensions.data();
    create_info.enabledLayerCount = use_validation_layers ? static_cast<uint32_t>(g_validation_layers.size()) : 0;
    create_info.ppEnabledLayerNames = use_validation_layers ? g_validation_layers.data() : nullptr;

    // create the vulkan instance
    vkCheck(vkCreateInstance(&create_info, nullptr, &m_handle), "failed to create instance!");

    if(use_validation_layers){ setup_debug_callback(); }
    m_extensions = the_required_extensions;

    uint32_t num_devices = 0;
    vkEnumeratePhysicalDevices(m_handle, &num_devices, nullptr);

    if(!num_devices){ return false; }

    m_physical_devices.resize(num_devices);
    vkEnumeratePhysicalDevices(m_handle, &num_devices, m_physical_devices.data());

    for(const auto &device : m_physical_devices)
    {
        VkPhysicalDeviceProperties device_props = {};
        vkGetPhysicalDeviceProperties(device, &device_props);
        auto version_major = VK_VERSION_MAJOR(device_props.apiVersion);
        auto version_minor = VK_VERSION_MINOR(device_props.apiVersion);
        auto version_patch = VK_VERSION_PATCH(device_props.apiVersion);
        LOG_INFO << "device: " << device_props.deviceName;
        LOG_INFO << "API-version: " << version_major << "." << version_minor << " (patch: " << version_patch << ")";
    }
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

void Instance::setup_debug_callback()
{
    if(m_debug_callback)
    {
        DestroyDebugReportCallbackEXT(m_handle, m_debug_callback, nullptr);
        m_debug_callback = nullptr;
    }

    VkDebugReportCallbackCreateInfoEXT create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT; // | VK_DEBUG_REPORT_DEBUG_BIT_EXT
    create_info.pUserData = &m_debug_fn;
    create_info.pfnCallback = debug_callback;

    vkCheck(CreateDebugReportCallbackEXT(m_handle, &create_info, nullptr, &m_debug_callback),
            "failed to set up debug callback!");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void swap(Instance &lhs, Instance &rhs)
{
    std::swap(lhs.m_extensions, rhs.m_extensions);
    std::swap(lhs.m_handle, rhs.m_handle);
    std::swap(lhs.m_physical_devices, rhs.m_physical_devices);
    std::swap(lhs.m_debug_callback, rhs.m_debug_callback);
    std::swap(lhs.m_debug_fn, rhs.m_debug_fn);
}

}//namespace vulkan