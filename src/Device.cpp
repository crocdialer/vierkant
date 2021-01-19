//
// Created by crocdialer on 2/8/19.
//

#include <set>

#define VMA_IMPLEMENTATION

#include <vierkant/vk_mem_alloc.h>

#include <vierkant/Device.hpp>

namespace vierkant
{

////////////////////////////// VALIDATION LAYER ///////////////////////////////////////////////////

const std::vector<const char *> g_validation_layers = {"VK_LAYER_LUNARG_standard_validation"};

///////////////////////////////////////////////////////////////////////////////////////////////////

const std::vector<VkQueue> g_empty_queue;

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

std::map<Device::Queue, Device::queue_family_info_t> find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    std::map<Device::Queue, Device::queue_family_info_t> indices = {};

    uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, queue_families.data());

    int i = 0;
    for(const auto &queueFamily : queue_families)
    {
        if(queueFamily.queueCount > 0)
        {
            if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices[Device::Queue::GRAPHICS].index = i;
                VkBool32 present_support = static_cast<VkBool32>(false);
                if(surface){ vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support); }

                if(present_support)
                {
                    indices[Device::Queue::PRESENT].index = i;
                    indices[Device::Queue::PRESENT].num_queues = queueFamily.queueCount;
                }
                else{ indices[Device::Queue::PRESENT].num_queues = 0; }
                indices[Device::Queue::GRAPHICS].num_queues = queueFamily.queueCount;
            }
            if(queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                indices[Device::Queue::TRANSFER].index = i;
                indices[Device::Queue::TRANSFER].num_queues = queueFamily.queueCount;
            }
            if(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                indices[Device::Queue::COMPUTE].index = i;
                indices[Device::Queue::COMPUTE].num_queues = queueFamily.queueCount;
            }
        }
        i++;
    }
    return indices;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

DevicePtr Device::create(const create_info_t &create_info)
{
    return DevicePtr(new Device(create_info));
}


///////////////////////////////////////////////////////////////////////////////////////////////////

Device::Device(const create_info_t &create_info) :
        m_physical_device(create_info.physical_device)
{
    if(!create_info.instance){ throw std::runtime_error("vierkant::Device::create_info_t::instance NOT set"); }
    if(!create_info.physical_device)
    {
        throw std::runtime_error("vierkant::Device::create_info_t::physical_device NOT set");
    }

    // query physical device properties
    m_physical_device_properties = {};
    m_physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(create_info.physical_device, &m_physical_device_properties);

    // add some obligatory features here
    VkPhysicalDeviceFeatures device_features = create_info.device_features;
    device_features.geometryShader = true;
    device_features.samplerAnisotropy = true;
    device_features.sampleRateShading = true;
    device_features.independentBlend = true;

    std::vector<const char *> extensions;
    for(const auto &ext : create_info.extensions){ extensions.push_back(ext); }
    if(create_info.surface){ extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); }

    m_queue_indices = find_queue_families(m_physical_device, create_info.surface);

    uint32_t num_queue_families;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &num_queue_families, nullptr);
    std::vector<VkQueueFamilyProperties> family_props(num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &num_queue_families, family_props.data());

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<int> unique_queue_families = {m_queue_indices[Device::Queue::GRAPHICS].index,
                                           m_queue_indices[Device::Queue::TRANSFER].index,
                                           m_queue_indices[Device::Queue::COMPUTE].index,
                                           m_queue_indices[Device::Queue::PRESENT].index};
    // helper to pass priorities
    std::vector<std::vector<float>> queue_priorities;

    for(int queue_family : unique_queue_families)
    {
        if(queue_family >= 0)
        {
            size_t queue_count = family_props[queue_family].queueCount;
            std::vector<float> tmp_priorities(queue_count, 1.f);
            VkDeviceQueueCreateInfo queue_create_info = {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = queue_count;
            queue_create_info.pQueuePriorities = tmp_priorities.data();
            queue_create_infos.push_back(queue_create_info);
            queue_priorities.push_back(std::move(tmp_priorities));
        }
    }

    // optional feature 'VkDeviceAddress'
    VkPhysicalDeviceBufferDeviceAddressFeatures device_address_features = {};
    device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    device_address_features.pNext = create_info.create_device_pNext;
    device_address_features.bufferDeviceAddress = static_cast<VkBool32>(true);

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    // pNext feature chaining
    device_create_info.pNext = create_info.enable_device_address ? &device_address_features
                                                                 : create_info.create_device_pNext;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = queue_create_infos.size();
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    device_create_info.ppEnabledExtensionNames = extensions.data();

    if(create_info.use_validation)
    {
        device_create_info.enabledLayerCount = static_cast<uint32_t>(g_validation_layers.size());
        device_create_info.ppEnabledLayerNames = g_validation_layers.data();
    }
    else{ device_create_info.enabledLayerCount = 0; }

    vkCheck(vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device),
            "failed to create logical device!");

    auto get_all_queues = [this](Queue type)
    {
        if(m_queue_indices[type].index >= 0)
        {
            auto num_queues = m_queue_indices[type].num_queues;

            m_queues[type].resize(num_queues);

            for(uint32_t i = 0; i < num_queues; ++i)
            {
                vkGetDeviceQueue(m_device, static_cast<uint32_t>(m_queue_indices[type].index), i,
                                 &m_queues[type][i]);
            }
        }
    };
    Queue queue_types[] = {Queue::GRAPHICS, Queue::TRANSFER, Queue::COMPUTE, Queue::PRESENT};
    for(auto q : queue_types){ get_all_queues(q); }

    // command pools
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // regular command pool -> graphics queue
    pool_info.queueFamilyIndex = static_cast<uint32_t>(m_queue_indices[Queue::GRAPHICS].index);
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCheck(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool),
            "failed to create command pool!");

    // transient command pool -> graphics queue
    pool_info.queueFamilyIndex = static_cast<uint32_t>(m_queue_indices[Queue::GRAPHICS].index);
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCheck(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool_transient),
            "failed to create command pool!");

    // transfer command pool -> transfer queue
    pool_info.queueFamilyIndex = static_cast<uint32_t>(m_queue_indices[Queue::TRANSFER].index);
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCheck(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool_transfer),
            "failed to create command pool!");

    // create VMA allocator instance
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.vulkanApiVersion = Instance::api_version;
    allocator_info.instance = create_info.instance;
    allocator_info.physicalDevice = m_physical_device;
    allocator_info.device = m_device;

    // optionally enable DEVICE_ADDRESS_BIT
    if(create_info.enable_device_address){ allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; }

    vmaCreateAllocator(&allocator_info, &m_vk_mem_allocator);

    m_max_usable_samples = max_usable_sample_count(m_physical_device);
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

VkQueue Device::queue(Queue type) const
{
    auto queue_it = m_queues.find(type);

    if(queue_it != m_queues.end() && !queue_it->second.empty())
    {
        return queue_it->second.front();
    }
    return VK_NULL_HANDLE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const std::vector<VkQueue> &Device::queues(Queue type) const
{
    auto queue_it = m_queues.find(type);

    if(queue_it != m_queues.end()){ return queue_it->second; }
    return g_empty_queue;
}


}//namespace vulkan