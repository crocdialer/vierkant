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
class RayTracer
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

        //! binary blob for push-constants
        std::vector<uint8_t> push_constants;
    };

    struct create_info_t
    {
        uint32_t num_frames_in_flight = 1;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
    };

    //! return an array listing required device-extensions for a raytracing-pipeline.
    static std::vector<const char *> required_extensions();

    RayTracer() = default;

    explicit RayTracer(const vierkant::DevicePtr &device, const create_info_t &create_info);

    RayTracer(RayTracer &&other) noexcept;

    RayTracer(const RayTracer &) = delete;

    RayTracer &operator=(RayTracer other);

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &properties() const { return m_properties; };

    /**
     * @brief   trace_rays invokes a raytracing pipeline.
     *
     * @param   tracable
     */
    void trace_rays(tracable_t tracable, VkCommandBuffer commandbuffer);

    /**
     * @return  the current frame-index.
     */
    [[nodiscard]] uint32_t current_index() const { return m_current_index; }

    /**
     * @return  the number of concurrent (in-flight) frames.
     */
    [[nodiscard]] uint32_t num_concurrent_frames() const { return static_cast<uint32_t>(m_trace_assets.size()); }

    friend void swap(RayTracer &lhs, RayTracer &rhs) noexcept;

private:
    struct trace_assets_t
    {
        //! keep passed tracable
        tracable_t tracable;

        //! cache used descriptor-sets
        vierkant::descriptor_set_map_t descriptor_set_cache;

        //! cache used descriptor-set-layouts
        std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> descriptor_layout_cache;
    };

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
                VkStridedDeviceAddressRegionKHR raygen;
                VkStridedDeviceAddressRegionKHR hit;
                VkStridedDeviceAddressRegionKHR miss;
                VkStridedDeviceAddressRegionKHR callable;
            };
            VkStridedDeviceAddressRegionKHR strided_address_region[Group::MAX_ENUM];
        };
    };

    shader_binding_table_t create_shader_binding_table(VkPipeline pipeline,
                                                       const vierkant::raytracing_shader_map_t &shader_stages);

    vierkant::DevicePtr m_device;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_properties = {};

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    crocore::Cache_<VkPipeline, shader_binding_table_t> m_binding_tables;

    std::vector<trace_assets_t> m_trace_assets;

    uint32_t m_current_index = 0;
};

}// namespace vierkant
