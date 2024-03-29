//
// Created by crocdialer on 11/15/20.
//

#pragma once

#include <vierkant/Device.hpp>
#include <vierkant/PipelineCache.hpp>
#include <vierkant/descriptor.hpp>

namespace vierkant
{

static inline uint32_t group_count(uint32_t thread_count, uint32_t local_size)
{
    return local_size ? (thread_count + local_size - 1) / local_size : 0;
}

/**
 * @brief   Compute can be used to run compute-pipelines.
 *
 */
class Compute
{
public:
    struct computable_t
    {
        //! information for a raytracing pipeline
        compute_pipeline_info_t pipeline_info = {};

        //! dimensions for compute-invocation
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
    void dispatch(std::vector<computable_t> computables, VkCommandBuffer commandbuffer);

    inline explicit operator bool() const { return static_cast<bool>(m_device && !m_compute_assets.empty()); };

    friend void swap(Compute &lhs, Compute &rhs) noexcept;

private:
    struct compute_assets_t
    {
        //! keep passed computables
        std::vector<computable_t> computables;

        //! cache used descriptor-sets
        vierkant::descriptor_set_map_t descriptor_set_cache;

        //! cache used descriptor-set-layouts
        std::unordered_map<descriptor_map_t, DescriptorSetLayoutPtr> descriptor_layout_cache;
    };

    vierkant::DevicePtr m_device;

    vierkant::DescriptorPoolPtr m_descriptor_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    std::vector<compute_assets_t> m_compute_assets;

    uint32_t m_current_index = 0;
};

}// namespace vierkant
