//
// Created by crocdialer on 2/8/19.
//

#include <set>

//-Wreorder
//-Wunused-variable
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "../include/vierkant/Device.hpp"

namespace vierkant {

////////////////////////////// VALIDATION LAYER ///////////////////////////////////////////////////

const std::vector<const char*> g_validation_layers = { "VK_LAYER_LUNARG_standard_validation" };

///////////////////////////////////////////////////////////////////////////////////////////////////

VkSampleCountFlagBits max_usable_sample_count(VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physical_device, &physicalDeviceProperties);

    VkSampleCountFlags counts = std::min(physicalDeviceProperties.limits.framebufferColorSampleCounts,
                                         physicalDeviceProperties.limits.framebufferDepthSampleCounts);
    if(counts & VK_SAMPLE_COUNT_64_BIT){ return VK_SAMPLE_COUNT_64_BIT; }
    if(counts & VK_SAMPLE_COUNT_32_BIT){ return VK_SAMPLE_COUNT_32_BIT; }
    if(counts & VK_SAMPLE_COUNT_16_BIT){ return VK_SAMPLE_COUNT_16_BIT; }
    if(counts & VK_SAMPLE_COUNT_8_BIT){ return VK_SAMPLE_COUNT_8_BIT; }
    if(counts & VK_SAMPLE_COUNT_4_BIT){ return VK_SAMPLE_COUNT_4_BIT; }
    if(counts & VK_SAMPLE_COUNT_2_BIT){ return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Device::QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    Device::QueueFamilyIndices indices = {};

    uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, queue_families.data());

    int i = 0;
    for(const auto& queueFamily : queue_families)
    {
        if(queueFamily.queueCount > 0)
        {
            if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){ indices.graphics_family = i; }
            if(queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT){ indices.transfer_family = i; }
            if(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT){ indices.compute_family = i; }
        }
        VkBool32 present_support = static_cast<VkBool32>(false);
        if(surface){ vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support); }
        if(present_support){ indices.present_family = i; }

        if(indices.graphics_family >= 0 && indices.present_family >=0){ break; }
        i++;
    }
    return indices;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DevicePtr Device::create(VkPhysicalDevice pd, bool use_validation, VkSurfaceKHR surface,
                         VkPhysicalDeviceFeatures device_features)
{
    return DevicePtr(new Device(pd, use_validation, surface, device_features));
}


///////////////////////////////////////////////////////////////////////////////////////////////////

Device::Device(VkPhysicalDevice physical_device, bool use_validation_layers, VkSurfaceKHR surface,
               VkPhysicalDeviceFeatures device_features):
m_physical_device(physical_device)
{
    if(physical_device)
    {
        // add some obligatory features here
        device_features.samplerAnisotropy = VK_TRUE;

        std::vector<const char*> extensions;
        if(surface){ extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); }

        m_queue_family_indices = find_queue_families(physical_device, surface);

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<int> unique_queue_families = {m_queue_family_indices.graphics_family,
                                               m_queue_family_indices.transfer_family,
                                               m_queue_family_indices.compute_family,
                                               m_queue_family_indices.present_family};
        float queue_priority = 1.0f;

        for(int queue_family : unique_queue_families)
        {
            if(queue_family >= 0)
            {
                VkDeviceQueueCreateInfo queue_create_info = {};
                queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_create_info.queueFamilyIndex = queue_family;
                queue_create_info.queueCount = 1;
                queue_create_info.pQueuePriorities = &queue_priority;
                queue_create_infos.push_back(queue_create_info);
            }
        }
        VkDeviceCreateInfo device_create_info = {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.queueCreateInfoCount = queue_create_infos.size();
        device_create_info.pEnabledFeatures = &device_features;
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        device_create_info.ppEnabledExtensionNames = extensions.data();

        if(use_validation_layers)
        {
            device_create_info.enabledLayerCount = static_cast<uint32_t>(g_validation_layers.size());
            device_create_info.ppEnabledLayerNames = g_validation_layers.data();
        }
        else{ device_create_info.enabledLayerCount = 0; }

        vkCheck(vkCreateDevice(physical_device, &device_create_info, nullptr, &m_device),
                "failed to create logical device!");

        auto get_queue = [this](VkQueue *queue, int index)
        {
            if(index >= 0){ vkGetDeviceQueue(m_device, static_cast<uint32_t>(index), 0, queue); }
        };
        get_queue(&m_graphics_queue, m_queue_family_indices.graphics_family);
        get_queue(&m_transfer_queue, m_queue_family_indices.transfer_family);
        get_queue(&m_compute_queue, m_queue_family_indices.compute_family);
        get_queue(&m_present_queue, m_queue_family_indices.present_family);

        // command pools
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        // regular command pool -> graphics queue
        pool_info.queueFamilyIndex = static_cast<uint32_t>(queue_family_indices().graphics_family);
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCheck(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool),
                "failed to create command pool!");

        // transient command pool -> graphics queue
        pool_info.queueFamilyIndex = static_cast<uint32_t>(queue_family_indices().graphics_family);
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCheck(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool_transient),
                "failed to create command pool!");

        // transient command pool -> transfer queue
        pool_info.queueFamilyIndex = static_cast<uint32_t>(queue_family_indices().transfer_family);
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCheck(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool_transfer),
                "failed to create command pool!");

        // create VMA allocator instance
        VmaAllocatorCreateInfo allocator_info = {};
        allocator_info.physicalDevice = physical_device;
        allocator_info.device = m_device;
        vmaCreateAllocator(&allocator_info, &m_vk_mem_allocator);

        m_max_usable_samples = max_usable_sample_count(physical_device);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Device::~Device()
{
    if(m_command_pool_transfer)
    {
        vkDestroyCommandPool(m_device, m_command_pool_transfer, nullptr);
        m_command_pool_transfer = nullptr;
    }
    if(m_command_pool_transient)
    {
        vkDestroyCommandPool(m_device, m_command_pool_transient, nullptr);
        m_command_pool_transient = nullptr;
    }
    if(m_command_pool)
    {
        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        m_command_pool = nullptr;
    }
    if(m_vk_mem_allocator)
    {
        vmaDestroyAllocator(m_vk_mem_allocator);
        m_vk_mem_allocator = nullptr;
    }
    if(m_device)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace vulkan