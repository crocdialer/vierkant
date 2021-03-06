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
        // read-only locked for searching
        {
            std::shared_lock<std::shared_mutex> lock(m_pipeline_mutex);
            auto it = m_pipelines.find(format);

            // found
            if(it != m_pipelines.end()){ return it->second; }
        }

        // not found -> create pipeline
        auto new_pipeline = Pipeline::create(m_device, format);

        std::unique_lock<std::shared_mutex> lock(m_pipeline_mutex);
        auto pipe_it = m_pipelines.insert(std::make_pair(format, std::move(new_pipeline))).first;
        return pipe_it->second;
    }

    /**
     * @brief   Retrieve a raytracing-pipeline from the cache. Will create and cache a new pipeline, if necessary.
     *
     * @param   format  a raytracing_pipeline_info_t describing the requested pipeline
     * @return  a const ref to a shared vierkant::Pipeline
     */
    const PipelinePtr &pipeline(const raytracing_pipeline_info_t &format)
    {
        // read-only locked for searching
        {
            std::shared_lock<std::shared_mutex> lock(m_ray_pipeline_mutex);
            auto it = m_ray_pipelines.find(format);

            // found
            if(it != m_ray_pipelines.end()){ return it->second; }
        }

        // not found -> create pipeline
        auto new_pipeline = Pipeline::create(m_device, format);

        std::unique_lock<std::shared_mutex> lock(m_ray_pipeline_mutex);
        auto pipe_it = m_ray_pipelines.insert(std::make_pair(format, std::move(new_pipeline))).first;
        return pipe_it->second;
    }

    const vierkant::shader_stage_map_t &shader_stages(ShaderType shader_type)
    {
        // read-only locked for searching
        {
            std::shared_lock<std::shared_mutex> lock(m_shader_stage_mutex);
            auto it = m_shader_stages.find(shader_type);

            // found
            if(it != m_shader_stages.end()){ return it->second; }
        }

        // not found -> create pipeline
        auto new_shader_stages = vierkant::create_shader_stages(m_device, shader_type);

        std::unique_lock<std::shared_mutex> lock(m_shader_stage_mutex);
        auto shader_stage_it = m_shader_stages.insert(std::make_pair(shader_type, std::move(new_shader_stages))).first;
        return shader_stage_it->second;
    }

    void clear()
    {
        {
            std::unique_lock<std::shared_mutex> lock(m_pipeline_mutex);
            m_pipelines.clear();
        }
        std::unique_lock<std::shared_mutex> ray_lock(m_ray_pipeline_mutex);
        m_ray_pipelines.clear();
    }

private:

    explicit PipelineCache(vierkant::DevicePtr device) : m_device(std::move(device)){}

    vierkant::DevicePtr m_device;

    std::shared_mutex m_pipeline_mutex, m_ray_pipeline_mutex;

    std::unordered_map<graphics_pipeline_info_t, PipelinePtr> m_pipelines;
    std::unordered_map<raytracing_pipeline_info_t, PipelinePtr> m_ray_pipelines;

    std::shared_mutex m_shader_stage_mutex;

    std::unordered_map<vierkant::ShaderType, vierkant::shader_stage_map_t> m_shader_stages;
};
}