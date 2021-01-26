//
// Created by crocdialer on 11/15/20.
//

#include <vierkant/Raytracer.hpp>

namespace vierkant
{

inline uint32_t aligned_size(uint32_t size, uint32_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

inline VkTransformMatrixKHR vk_transform_matrix(const glm::mat4 &m)
{
    VkTransformMatrixKHR ret;
    auto transpose = glm::transpose(m);
    memcpy(&ret, glm::value_ptr(transpose), sizeof(VkTransformMatrixKHR));
    return ret;
}

std::vector<const char *> Raytracer::required_extensions()
{
    return {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
}

Raytracer::Raytracer(const vierkant::DevicePtr &device) :
        m_device(device),
        m_pipeline_cache(vierkant::PipelineCache::create(device))
{
    // get the ray tracing and acceleration-structure related function pointers
    set_function_pointers();

    // query the ray tracing properties
    m_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProps2{};
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &m_properties;
    vkGetPhysicalDeviceProperties2(m_device->physical_device(), &deviceProps2);

    m_command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // we also need a DescriptorPool ...
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 32},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              32},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             128},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             256}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 512);
}

void Raytracer::set_function_pointers()
{
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(
            m_device->handle(), "vkGetRayTracingShaderGroupHandlesKHR"));

    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_device->handle(),
                                                                                    "vkCmdTraceRaysKHR"));
}

void Raytracer::trace_rays(tracable_t tracable, VkCommandBuffer commandbuffer)
{
    // create or retrieve an existing raytracing pipeline
    auto pipeline = m_pipeline_cache->pipeline(tracable.pipeline_info);

//    // TODO: cache
//    auto descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device, tracable.descriptors);
//    VkDescriptorSetLayout set_layout_handle = descriptor_set_layout.get();
//
//    tracable.pipeline_info.descriptor_set_layouts = {set_layout_handle};

    // create the binding table
    shader_binding_table_t binding_table = {};

    try{ binding_table = m_binding_tables.get(pipeline->handle()); }
    catch(std::out_of_range &e)
    {
        binding_table = m_binding_tables.put(pipeline->handle(),
                                             create_shader_binding_table(pipeline->handle(),
                                                                         tracable.pipeline_info.shader_stages));
    }

    // fetch descriptor set
    DescriptorSetPtr descriptor_set;
    try{ descriptor_set = m_descriptor_sets.get(tracable.descriptor_set_layout); }
    catch(std::out_of_range &e)
    {
        descriptor_set = m_descriptor_sets.put(tracable.descriptor_set_layout,
                                               vierkant::create_descriptor_set(m_device, m_descriptor_pool,
                                                                               tracable.descriptor_set_layout));
        // update descriptor-set with actual descriptors
        vierkant::update_descriptor_set(m_device, descriptor_set, tracable.descriptors);
    }

    VkDescriptorSet descriptor_set_handle = descriptor_set.get();

    vierkant::CommandBuffer local_commandbuffer;

    if(commandbuffer == VK_NULL_HANDLE)
    {
        local_commandbuffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
        local_commandbuffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        commandbuffer = local_commandbuffer.handle();
    }

    // bind raytracing pipeline
    pipeline->bind(commandbuffer);

    // bind descriptor set (accelearation-structure, uniforms, storage-buffers, samplers, storage-image)
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->layout(),
                            0, 1, &descriptor_set_handle, 0, nullptr);

    // finally record the tracing command
    vkCmdTraceRaysKHR(commandbuffer,
                      &binding_table.raygen,
                      &binding_table.miss,
                      &binding_table.hit,
                      &binding_table.callable,
                      tracable.extent.width, tracable.extent.height, tracable.extent.depth);

    // submit only if we created the command buffer
    if(local_commandbuffer){ local_commandbuffer.submit(m_device->queue(), true); }
}

Raytracer::shader_binding_table_t
Raytracer::create_shader_binding_table(VkPipeline pipeline,
                                       const vierkant::raytracing_shader_map_t &shader_stages)
{
    // shader groups
    auto group_create_infos = vierkant::raytracing_shader_groups(shader_stages);

    const uint32_t group_count = group_create_infos.size();
    const uint32_t handle_size = m_properties.shaderGroupHandleSize;
    const uint32_t handle_size_aligned = aligned_size(handle_size, m_properties.shaderGroupBaseAlignment);
    const uint32_t binding_table_size = group_count * handle_size_aligned;

    // retrieve the shader-handles into host-memory
    std::vector<uint8_t> shader_handle_data(group_count * handle_size);
    vkCheck(vkGetRayTracingShaderGroupHandlesKHR(m_device->handle(), pipeline, 0, group_count,
                                                 shader_handle_data.size(),
                                                 shader_handle_data.data()),
            "Raytracer::trace_rays: could not retrieve shader group handles");

    shader_binding_table_t binding_table = {};
    binding_table.buffer = vierkant::Buffer::create(m_device, nullptr, binding_table_size,
                                                    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

    // copy opaque shader-handles with proper stride (handle_size_aligned)
    auto buf_ptr = static_cast<uint8_t *>(binding_table.buffer->map());
    for(uint32_t i = 0; i < group_count; ++i)
    {
        memcpy(buf_ptr + i * handle_size_aligned, shader_handle_data.data() + i * handle_size, handle_size);
    }
    binding_table.buffer->unmap();

    // this feels a bit silly but these groups do not correspond 1:1 to shader-stages.
    std::map<shader_binding_table_t::Group, size_t> group_elements;
    for(const auto &[stage, shader] : shader_stages)
    {
        switch(stage)
        {
            case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                group_elements[shader_binding_table_t::Group::Raygen]++;
                break;

            case VK_SHADER_STAGE_MISS_BIT_KHR:
                group_elements[shader_binding_table_t::Group::Miss]++;
                break;

            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                group_elements[shader_binding_table_t::Group::Hit]++;
                break;

            case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
                group_elements[shader_binding_table_t::Group::Callable]++;
                break;

            default:
                break;
        }
    }

    size_t buffer_offset = 0;

    for(const auto &[group, num_elements] : group_elements)
    {
        auto &address_region = binding_table.strided_address_region[group];
        address_region.deviceAddress = binding_table.buffer->device_address() + buffer_offset;
        address_region.stride = handle_size_aligned;
        address_region.size = handle_size_aligned * num_elements;

        buffer_offset += address_region.size;
    }
    return binding_table;
}

}//namespace vierkant
