//
// Created by crocdialer on 11/15/20.
//

#pragma once

#include <crocore/Cache.hpp>

#include <vierkant/Device.hpp>
#include <vierkant/PipelineCache.hpp>
#include <vierkant/descriptor.hpp>

namespace vierkant
{

static inline uint32_t group_count(uint32_t thread_count, uint32_t local_size)
{
    return (thread_count + local_size - 1) / local_size;
}

/**
 * @brief   Raytracer can be used to run raytracing pipelines.
 *
 */
class Compute
{
public:

    struct computable_t
    {
        //! information for a raytracing pipeline
        compute_pipeline_info_t pipeline_info = {};

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
        vierkant::CommandPoolPtr command_pool;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
    };

    Compute() = default;

    explicit Compute(const vierkant::DevicePtr &device, const create_info_t &create_info);

    Compute(Compute &&other) noexcept;

    Compute(const Compute &) = delete;

    Compute &operator=(Compute other);

    /**
     * @brief   trace_rays invokes a raytracing pipeline.
     *
     * @param   tracable
     */
    void dispatch(std::vector<computable_t> computables, VkCommandBuffer commandbuffer = VK_NULL_HANDLE);

    friend void swap(Compute &lhs, Compute &rhs) noexcept;

private:

    struct compute_assets_t
    {
        //! keep passed computables
        std::vector<computable_t> computables;

        crocore::Cache_<DescriptorSetLayoutPtr, DescriptorSetPtr> descriptor_sets;
    };

    vierkant::DevicePtr m_device;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    std::vector<compute_assets_t> m_compute_assets;

    std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> m_descriptor_set_layouts;

    uint32_t m_current_index = 0;
};

}// namespace vierkant

