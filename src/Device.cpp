#include <set>
#include <vierkant/Device.hpp>
#include <vierkant/git_hash.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace vierkant
{

constexpr char g_portability_ext_name[] = "VK_KHR_portability_subset";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QueryPoolPtr create_query_pool(const vierkant::DevicePtr &device, uint32_t query_count, VkQueryType query_type)
{
    VkQueryPoolCreateInfo pool_create_info = {};
    pool_create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;

    pool_create_info.queryCount = query_count;
    pool_create_info.queryType = query_type;

    VkQueryPool handle = VK_NULL_HANDLE;
    vkCheck(vkCreateQueryPool(device->handle(), &pool_create_info, nullptr, &handle), "could not create VkQueryPool");
    vkResetQueryPool(device->handle(), handle, 0, query_count);
    return {handle, [device](VkQueryPool p) { vkDestroyQueryPool(device->handle(), p, nullptr); }};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string device_info(VkPhysicalDevice physical_device)
{
    // query physical device properties
    VkPhysicalDeviceProperties2 physical_device_properties = {};
    physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties);

    auto version_major = VK_API_VERSION_MAJOR(physical_device_properties.properties.apiVersion);
    auto version_minor = VK_API_VERSION_MINOR(physical_device_properties.properties.apiVersion);
    auto version_patch = VK_API_VERSION_PATCH(physical_device_properties.properties.apiVersion);

    std::string driver_info = "unknown";

    // nvidia
    if(physical_device_properties.properties.vendorID == 0x10de)
    {
        // 10|8|8|6
        uint32_t versionraw = physical_device_properties.properties.driverVersion;
        uint32_t nvidia_driver_major = (versionraw >> 22) & 0x3ff;
        uint32_t nvidia_driver_minor = (versionraw >> 14) & 0x0ff;
        uint32_t nvidia_driver_patch = (versionraw >> 6) & 0x0ff;
        driver_info = fmt::format("{}.{}.{:02}", nvidia_driver_major, nvidia_driver_minor, nvidia_driver_patch);
    }
    // AMD ...
    else
    {
        // default version-schema is 10|10|12
        uint32_t versionraw = physical_device_properties.properties.driverVersion;
        driver_info = fmt::format("{}.{}.{:02}", VK_VERSION_MAJOR(versionraw), VK_VERSION_MINOR(versionraw),
                                  VK_VERSION_PATCH(versionraw));
    }
    return fmt::format("Vulkan {}.{}.{} - {} (driver: {}) - vierkant: {} | {}", version_major, version_minor,
                       version_patch, physical_device_properties.properties.deviceName, driver_info, GIT_COMMIT_HASH,
                       GIT_COMMIT_DATE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkPhysicalDeviceProperties2 device_properties(VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties2 device_props = {};
    device_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(physical_device, &device_props);
    return device_props;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double timestamp_millis(const uint64_t *timestamps, int32_t idx, float timestamp_period)
{
    using double_millisecond_t = std::chrono::duration<double, std::milli>;
    size_t lhs = 2 * idx, rhs = 2 * idx + 1;
    auto frame_ns = std::chrono::nanoseconds(
            static_cast<uint64_t>(double(timestamps[rhs] - timestamps[lhs]) * timestamp_period));
    return std::chrono::duration_cast<double_millisecond_t>(frame_ns).count();
}

////////////////////////////// VALIDATION LAYER ////////////////////////////////////////////////////////////////////////

const char *g_validation_layers[] = {"VK_LAYER_KHRONOS_validation"};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const std::vector<VkQueue> g_empty_queue;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkSampleCountFlagBits max_usable_sample_count(VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physical_device, &physicalDeviceProperties);

    VkSampleCountFlags counts = std::min(physicalDeviceProperties.limits.framebufferColorSampleCounts,
                                         physicalDeviceProperties.limits.framebufferDepthSampleCounts);
    if(counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if(counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if(counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if(counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if(counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if(counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::map<Device::Queue, Device::queue_family_info_t> find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    std::map<Device::Queue, Device::queue_family_info_t> indices = {};

    uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, queue_families.data());

    int i = 0;
    for(const auto &queueFamily: queue_families)
    {
        if(queueFamily.queueCount > 0)
        {
            if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices[Device::Queue::GRAPHICS].index = i;
                auto present_support = static_cast<VkBool32>(false);
                if(surface) { vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support); }

                if(present_support)
                {
                    indices[Device::Queue::PRESENT].index = i;
                    indices[Device::Queue::PRESENT].num_queues = queueFamily.queueCount;
                }
                else { indices[Device::Queue::PRESENT].num_queues = 0; }
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DevicePtr Device::create(const create_info_t &create_info) { return DevicePtr(new Device(create_info)); }


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Device::Device(const create_info_t &create_info) : m_physical_device(create_info.physical_device)
{
    if(!create_info.instance) { throw std::runtime_error("vierkant::Device::create_info_t::instance NOT set"); }
    if(!create_info.physical_device)
    {
        throw std::runtime_error("vierkant::Device::create_info_t::physical_device NOT set");
    }

    // query physical device properties
    {
        VkPhysicalDeviceProperties2 physical_device_properties = {};
        physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        m_properties.acceleration_structure.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
        m_properties.ray_pipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        m_properties.mesh_shader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
        m_properties.micromap_opacity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_PROPERTIES_EXT;
        m_properties.descriptor_buffer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
        physical_device_properties.pNext = &m_properties.acceleration_structure;
        m_properties.acceleration_structure.pNext = &m_properties.ray_pipeline;
        m_properties.ray_pipeline.pNext = &m_properties.mesh_shader;
        m_properties.mesh_shader.pNext = &m_properties.micromap_opacity;
        m_properties.micromap_opacity.pNext = &m_properties.descriptor_buffer;
        vkGetPhysicalDeviceProperties2(create_info.physical_device, &physical_device_properties);
        m_properties.core = physical_device_properties.properties;
    }

    auto extensions = create_info.extensions;
    if(create_info.surface) { extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); }

    // check if a portability-extension is available/required
    if(vierkant::check_device_extension_support(create_info.physical_device, {g_portability_ext_name}))
    {
        // if this extension is available, it's also mandatory
        extensions.push_back(g_portability_ext_name);
    }

    // check if mesh-shading was requested and if so, enable fragment-rate-shading as well.
    // this is for some weird reason required by primitive-culling in mesh-shaders
    if(crocore::contains(extensions, VK_EXT_MESH_SHADER_EXTENSION_NAME) &&
       vierkant::check_device_extension_support(create_info.physical_device,
                                                {VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME}))
    {
        extensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
    }

    if(!vierkant::check_device_extension_support(create_info.physical_device, extensions))
    {
        spdlog::critical("unsupported extension(s): {}", extensions);
    }

    m_queue_indices = find_queue_families(m_physical_device, create_info.surface);

    uint32_t num_queue_families;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &num_queue_families, nullptr);
    std::vector<VkQueueFamilyProperties> family_props(num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &num_queue_families, family_props.data());

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<int> unique_queue_families = {
            m_queue_indices[Device::Queue::GRAPHICS].index, m_queue_indices[Device::Queue::TRANSFER].index,
            m_queue_indices[Device::Queue::COMPUTE].index, m_queue_indices[Device::Queue::PRESENT].index};
    // helper to pass priorities
    std::vector<std::vector<float>> queue_priorities;

    for(int queue_family: unique_queue_families)
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

    // query Vulkan 1.1 features
    VkPhysicalDeviceVulkan11Features device_features_11 = {};
    device_features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

    // query Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features device_features_12 = {};
    device_features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    device_features_11.pNext = &device_features_12;

    // query Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features device_features_13 = {};
    device_features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    device_features_12.pNext = &device_features_13;

    void **pNext = &device_features_13.pNext;
    auto update_pnext = [&pNext, &extensions](const auto &feature_struct, const char *ext_name) {
        if(crocore::contains(extensions, ext_name))
        {
            *pNext = (void *) &feature_struct;
            pNext = (void **) &feature_struct.pNext;
        }
    };

    //------------------------------------ VK_KHR_fragment_shading_rate ------------------------------------------------
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragment_shading_rate_features = {};
    fragment_shading_rate_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
    update_pnext(fragment_shading_rate_features, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);

    //------------------------------------ VK_KHR_acceleration_structure -----------------------------------------------
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {};
    acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    update_pnext(acceleration_structure_features, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);

    //------------------------------------ VK_KHR_ray_tracing_pipeline -------------------------------------------------
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features = {};
    ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    update_pnext(ray_tracing_pipeline_features, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

    //------------------------------------ VK_KHR_ray_query ------------------------------------------------------------
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features = {};
    ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    update_pnext(ray_query_features, VK_KHR_RAY_QUERY_EXTENSION_NAME);

    //------------------------------------ VK_EXT_opacity_micromap -----------------------------------------------------
    VkPhysicalDeviceOpacityMicromapFeaturesEXT ray_micromap_features = {};
    ray_micromap_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT;
    update_pnext(ray_micromap_features, VK_EXT_OPACITY_MICROMAP_EXTENSION_NAME);

    //------------------------------------ VK_EXT_mesh_shader ----------------------------------------------------------
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features = {};
    mesh_shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    update_pnext(mesh_shader_features, VK_EXT_MESH_SHADER_EXTENSION_NAME);

    //------------------------------------ VK_EXT_descriptor_buffer ----------------------------------------------------
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features = {};
    descriptor_buffer_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    update_pnext(descriptor_buffer_features, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);

    //------------------------------------------------------------------------------------------------------------------
    *pNext = create_info.create_device_pNext;

    // query support for the required device-features
    VkPhysicalDeviceFeatures2 query_features = {};
    query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    query_features.pNext = &device_features_11;
    vkGetPhysicalDeviceFeatures2(m_physical_device, &query_features);

    mesh_shader_features.primitiveFragmentShadingRateMeshShader = false;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    // pNext feature chaining
    device_create_info.pNext = &device_features_11;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = queue_create_infos.size();

    // just enable all available features
    device_create_info.pEnabledFeatures = &query_features.features;
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    device_create_info.ppEnabledExtensionNames = extensions.data();

    if(create_info.use_validation)
    {
        device_create_info.enabledLayerCount =
                static_cast<uint32_t>(sizeof(g_validation_layers) / sizeof(const char *));
        device_create_info.ppEnabledLayerNames = g_validation_layers;
    }
    else { device_create_info.enabledLayerCount = 0; }

    spdlog::debug("device-extensions: {}", extensions);

    vkCheck(vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device),
            "failed to create logical device!");

    // short-circuit function-pointers to point directly add device/driver entries
    if(create_info.direct_function_pointers) { volkLoadDevice(m_device); }

    auto get_all_queues = [this](Queue type) {
        if(m_queue_indices[type].index >= 0)
        {
            auto num_queues = m_queue_indices[type].num_queues;

            m_queues[type].resize(num_queues);

            for(uint32_t i = 0; i < num_queues; ++i)
            {
                vkGetDeviceQueue(m_device, static_cast<uint32_t>(m_queue_indices[type].index), i, &m_queues[type][i]);
            }
        }
    };
    Queue queue_types[] = {Queue::GRAPHICS, Queue::TRANSFER, Queue::COMPUTE, Queue::PRESENT};
    for(auto q: queue_types) { get_all_queues(q); }

    // command pools
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

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
    VmaVulkanFunctions vma_vulkan_functions = {};
    vma_vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vma_vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.vulkanApiVersion = Instance::api_version;
    allocator_info.instance = create_info.instance;
    allocator_info.physicalDevice = m_physical_device;
    allocator_info.device = m_device;
    allocator_info.pVulkanFunctions = &vma_vulkan_functions;

    // optionally enable DEVICE_ADDRESS_BIT
    if(device_features_12.bufferDeviceAddress)
    {
        allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    vmaCreateAllocator(&allocator_info, &m_vk_mem_allocator);
    m_max_usable_samples = max_usable_sample_count(m_physical_device);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkQueue Device::queue(Queue type) const
{
    auto queue_it = m_queues.find(type);

    if(queue_it != m_queues.end() && !queue_it->second.empty()) { return queue_it->second.front(); }
    return VK_NULL_HANDLE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const std::vector<VkQueue> &Device::queues(Queue type) const
{
    auto queue_it = m_queues.find(type);

    if(queue_it != m_queues.end()) { return queue_it->second; }
    return g_empty_queue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Device::set_object_name(VkDeviceAddress handle, VkObjectType type, const std::string &name)
{
    if(vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT object_name_info = {};
        object_name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        object_name_info.objectType = type;
        object_name_info.objectHandle = handle;
        object_name_info.pObjectName = name.c_str();
        vkSetDebugUtilsObjectNameEXT(m_device, &object_name_info);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Device::wait_idle() { vkDeviceWaitIdle(m_device); }

}// namespace vierkant
