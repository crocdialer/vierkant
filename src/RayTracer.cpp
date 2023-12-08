//
// Created by crocdialer on 11/15/20.
//

#define VK_NO_PROTOTYPES
#include <volk.h>

#include <vierkant/RayTracer.hpp>

namespace vierkant
{

inline uint32_t aligned_size(uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); }

inline VkTransformMatrixKHR vk_transform_matrix(const glm::mat4 &m)
{
    VkTransformMatrixKHR ret;
    auto transpose = glm::transpose(m);
    memcpy(&ret, glm::value_ptr(transpose), sizeof(VkTransformMatrixKHR));
    return ret;
}

std::vector<const char *> RayTracer::required_extensions()
{
    return {VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RayTracer::RayTracer(RayTracer &&other) noexcept : RayTracer() { swap(*this, other); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RayTracer &RayTracer::operator=(RayTracer other)
{
    swap(*this, other);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void swap(RayTracer &lhs, RayTracer &rhs) noexcept
{
    if(&lhs == &rhs) { return; }

    std::swap(lhs.m_device, rhs.m_device);
    std::swap(lhs.m_descriptor_pool, rhs.m_descriptor_pool);
    std::swap(lhs.m_pipeline_cache, rhs.m_pipeline_cache);
    std::swap(lhs.m_binding_tables, rhs.m_binding_tables);
    std::swap(lhs.m_trace_assets, rhs.m_trace_assets);
    std::swap(lhs.m_current_index, rhs.m_current_index);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RayTracer::RayTracer(const vierkant::DevicePtr &device, const create_info_t &create_info)
    : m_device(device), m_pipeline_cache(create_info.pipeline_cache)
{
    if(!m_pipeline_cache) { m_pipeline_cache = vierkant::PipelineCache::create(device); }

    m_trace_assets.resize(create_info.num_frames_in_flight);

    if(create_info.descriptor_pool) { m_descriptor_pool = create_info.descriptor_pool; }
    else
    {
        vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 32},
                                                          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},
                                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128},
                                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256},
                                                          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512}};
        m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 128);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RayTracer::trace_rays(tracable_t tracable, VkCommandBuffer commandbuffer)
{
    auto &trace_asset = m_trace_assets[m_current_index];
    m_current_index = (m_current_index + 1) % m_trace_assets.size();
    vierkant::descriptor_set_map_t next_descriptor_set_cache;
    std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> next_layout_cache;

    auto descriptor_set_layout = vierkant::find_or_create_set_layout(
            m_device, tracable.descriptors, trace_asset.descriptor_layout_cache, next_layout_cache);
    tracable.pipeline_info.descriptor_set_layouts = {descriptor_set_layout.get()};

    // push constant range
    VkPushConstantRange push_constant_range = {};
    push_constant_range.offset = 0;
    push_constant_range.size = tracable.push_constants.size();
    push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
    tracable.pipeline_info.push_constant_ranges = {push_constant_range};

    // create or retrieve an existing raytracing pipeline
    auto pipeline = m_pipeline_cache->pipeline(tracable.pipeline_info);

    // create the binding table
    shader_binding_table_t binding_table = {};

    try
    {
        binding_table = m_binding_tables.get(pipeline->handle());
    } catch(std::out_of_range &e)
    {
        binding_table = m_binding_tables.put(
                pipeline->handle(),
                create_shader_binding_table(pipeline->handle(), tracable.pipeline_info.shader_stages));
    }

    // fetch descriptor set
    auto descriptor_set = vierkant::find_or_create_descriptor_set(m_device, descriptor_set_layout, tracable.descriptors,
                                                                  m_descriptor_pool, trace_asset.descriptor_set_cache,
                                                                  next_descriptor_set_cache, false);

    // update descriptor-set with actual descriptors
    vierkant::update_descriptor_set(m_device, tracable.descriptors, descriptor_set);

    VkDescriptorSet descriptor_set_handle = descriptor_set.get();

    // bind raytracing pipeline
    pipeline->bind(commandbuffer);

    // bind descriptor set (acceleration-structure, uniforms, storage-buffers, samplers, storage-image)
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->layout(), 0, 1,
                            &descriptor_set_handle, 0, nullptr);

    if(!tracable.push_constants.empty())
    {
        // update push_constants
        vkCmdPushConstants(commandbuffer, pipeline->layout(), VK_SHADER_STAGE_ALL, 0, tracable.push_constants.size(),
                           tracable.push_constants.data());
    }

    // finally record the tracing command
    vkCmdTraceRaysKHR(commandbuffer, &binding_table.raygen, &binding_table.miss, &binding_table.hit,
                      &binding_table.callable, tracable.extent.width, tracable.extent.height, tracable.extent.depth);

    // keep-alive of things in use
    trace_asset.tracable = std::move(tracable);
    trace_asset.descriptor_set_cache = std::move(next_descriptor_set_cache);
    trace_asset.descriptor_layout_cache = std::move(next_layout_cache);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RayTracer::shader_binding_table_t
RayTracer::create_shader_binding_table(VkPipeline pipeline, const vierkant::raytracing_shader_map_t &shader_stages)
{
    // shader groups
    auto group_create_infos = vierkant::raytracing_shader_groups(shader_stages);

    const uint32_t group_count = group_create_infos.size();
    const uint32_t handle_size = m_device->properties().ray_pipeline.shaderGroupHandleSize;
    const uint32_t handle_size_aligned =
            aligned_size(handle_size, m_device->properties().ray_pipeline.shaderGroupBaseAlignment);
    const uint32_t binding_table_size = group_count * handle_size_aligned;

    // retrieve the shader-handles into host-memory
    std::vector<uint8_t> shader_handle_data(group_count * handle_size);
    vkCheck(vkGetRayTracingShaderGroupHandlesKHR(m_device->handle(), pipeline, 0, group_count,
                                                 shader_handle_data.size(), shader_handle_data.data()),
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
    for(const auto &[stage, shader]: shader_stages)
    {
        switch(stage)
        {
            case VK_SHADER_STAGE_RAYGEN_BIT_KHR: group_elements[shader_binding_table_t::Group::Raygen]++; break;

            case VK_SHADER_STAGE_MISS_BIT_KHR: group_elements[shader_binding_table_t::Group::Miss]++; break;

            case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: group_elements[shader_binding_table_t::Group::Hit]++; break;

            case VK_SHADER_STAGE_CALLABLE_BIT_KHR: group_elements[shader_binding_table_t::Group::Callable]++; break;

            default: break;
        }
    }

    size_t buffer_offset = 0;

    for(const auto &[group, num_elements]: group_elements)
    {
        auto &address_region = binding_table.strided_address_region[group];
        address_region.deviceAddress = binding_table.buffer->device_address() + buffer_offset;
        address_region.stride = handle_size_aligned;
        address_region.size = handle_size_aligned * num_elements;

        buffer_offset += address_region.size;
    }
    return binding_table;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace vierkant
