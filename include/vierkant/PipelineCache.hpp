//
// Created by crocdialer on 7/5/19.
//

#pragma once

#include <shared_mutex>
#include <unordered_map>
#include "vierkant/Pipeline.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(PipelineCache)

/**
 * @brief   PipelineCache is used, well ... to cache pipelines and retrieve them in a thread-safe way.
 */
class PipelineCache
{
public:

    /**
     * @brief   Create a shared PipelineCache
     *
     * @param   device  handle for the vierkant::Device to create the pipelines with
     * @return  the newly created PipelineCachePtr
     */
    static PipelineCachePtr create(vierkant::DevicePtr device)
    {
        return PipelineCachePtr(new PipelineCache(std::move(device)));
    };

    PipelineCache(const PipelineCache &) = delete;

    PipelineCache(PipelineCache &&) = delete;

    PipelineCache &operator=(PipelineCache other) = delete;

    /**
     * @brief   Retrieve a graphics-pipeline from the cache. Will create and cache a new pipeline, if necessary.
     *
     * @param   format  a graphics_pipeline_info_t describing the requested pipeline
     * @return  a const ref to a shared vierkant::Pipeline
     */
    const PipelinePtr &pipeline(const graphics_pipeline_info_t &format)
    {
        return retrieve_pipeline(format, m_graphics_pipelines, m_graphics_pipeline_mutex);
    }

    /**
     * @brief   Retrieve a raytracing-pipeline from the cache. Will create and cache a new pipeline, if necessary.
     *
     * @param   format  a raytracing_pipeline_info_t describing the requested pipeline
     * @return  a const ref to a shared vierkant::Pipeline
     */
    const PipelinePtr &pipeline(const raytracing_pipeline_info_t &format)
    {
        return retrieve_pipeline(format, m_ray_pipelines, m_ray_pipeline_mutex);
    }

    /**
     * @brief   Retrieve a compute-pipeline from the cache. Will create and cache a new pipeline, if necessary.
     *
     * @param   format  a compute_pipeline_info_t describing the requested pipeline
     * @return  a const ref to a shared vierkant::Pipeline
     */
    const PipelinePtr &pipeline(const compute_pipeline_info_t &format)
    {
        return retrieve_pipeline(format, m_compute_pipelines, m_compute_pipeline_mutex);
    }

    const vierkant::shader_stage_map_t &shader_stages(ShaderType shader_type)
    {
        // read-only locked for searching
        {
            std::shared_lock lock(m_shader_stage_mutex);
            auto it = m_shader_stages.find(shader_type);

            // found
            if(it != m_shader_stages.end()){ return it->second; }
        }

        // not found -> create pipeline
        auto new_shader_stages = vierkant::create_shader_stages(m_device, shader_type);

        std::unique_lock lock(m_shader_stage_mutex);
        auto shader_stage_it = m_shader_stages.insert(std::make_pair(shader_type, std::move(new_shader_stages))).first;
        return shader_stage_it->second;
    }

    void clear()
    {
        {
            std::unique_lock<std::shared_mutex> lock(m_graphics_pipeline_mutex);
            m_graphics_pipelines.clear();
        }
        std::unique_lock<std::shared_mutex> ray_lock(m_ray_pipeline_mutex);
        m_ray_pipelines.clear();
    }

private:

    explicit PipelineCache(vierkant::DevicePtr device) : m_device(std::move(device)){}

    template<typename FMT_T>
    inline const PipelinePtr &retrieve_pipeline(const FMT_T &format,
                                                std::unordered_map<FMT_T, PipelinePtr> &map,
                                                std::shared_mutex &mutex)
    {
        // read-only locked for searching
        {
            std::shared_lock lock(mutex);
            auto it = map.find(format);

            // found
            if(it != map.end()){ return it->second; }
        }

        // not found -> create pipeline
        auto new_pipeline = Pipeline::create(m_device, format);

        // write-locked for insertion
        std::unique_lock write_lock(mutex);
        auto pipe_it = map.insert(std::make_pair(format, std::move(new_pipeline))).first;
        return pipe_it->second;
    }

    vierkant::DevicePtr m_device;

    std::shared_mutex m_graphics_pipeline_mutex, m_ray_pipeline_mutex, m_compute_pipeline_mutex;

    std::unordered_map<graphics_pipeline_info_t, PipelinePtr> m_graphics_pipelines;
    std::unordered_map<raytracing_pipeline_info_t, PipelinePtr> m_ray_pipelines;
    std::unordered_map<compute_pipeline_info_t, PipelinePtr> m_compute_pipelines;

    std::shared_mutex m_shader_stage_mutex;

    std::unordered_map<vierkant::ShaderType, vierkant::shader_stage_map_t> m_shader_stages;
};
}