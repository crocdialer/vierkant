//
// Created by crocdialer on 11/15/20.
//

#pragma once

#include <crocore/Cache.hpp>

#include <vierkant/Device.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/PipelineCache.hpp>
#include <vierkant/descriptor.hpp>

namespace vierkant
{

/**
 * @brief   Raytracer can be used to run raytracing pipelines.
 *
 */
class Raytracer
{
public:

    struct tracable_t
    {
        //! information for a raytracing pipeline
        raytracing_pipeline_info_t pipeline_info = {};

        //! dimensions for ray-generation
        VkExtent3D extent = {};

        //! a descriptormap
        descriptor_map_t descriptors;

        //! optional descriptor-set-layout
        DescriptorSetLayoutPtr descriptor_set_layout;
    };

    //! return an array listing all required device-extensions for a raytracing-pipeline.
    static std::vector<const char *> required_extensions();

    Raytracer() = default;

    explicit Raytracer(const vierkant::DevicePtr &device);

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &properties() const{ return m_properties; };

    /**
     * @brief   trace_rays invokes a raytracing pipeline.
     *
     * @param   tracable
     */
    void trace_rays(tracable_t tracable, VkCommandBuffer commandbuffer = VK_NULL_HANDLE);

private:

    struct shader_binding_table_t
    {
        vierkant::BufferPtr buffer;

        //! helper enum to create a shader-binding-table
        enum Group : uint32_t
        {
            Raygen = 0,
            Hit = 1,
            Miss = 2,
            Callable = 3,
            MAX_ENUM
        };

        union
        {
            struct
            {
                VkStridedDeviceAddressRegionKHR raygen = {};
                VkStridedDeviceAddressRegionKHR hit = {};
                VkStridedDeviceAddressRegionKHR miss = {};
                VkStridedDeviceAddressRegionKHR callable = {};
            };
            VkStridedDeviceAddressRegionKHR strided_address_region[Group::MAX_ENUM];
        };
    };

    shader_binding_table_t create_shader_binding_table(VkPipeline pipeline,
                                                       const vierkant::raytracing_shader_map_t &shader_stages);

    void set_function_pointers();

    vierkant::DevicePtr m_device;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_properties = {};

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    crocore::Cache_<VkPipeline, shader_binding_table_t> m_binding_tables;

    crocore::Cache_<DescriptorSetLayoutPtr, DescriptorSetPtr> m_descriptor_sets;

    // process-addresses for raytracing related functions
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
};

}// namespace vierkant

