//
// Created by crocdialer on 7/5/19.
//

#pragma once

#include <shared_mutex>
#include <unordered_map>
#include "vierkant/Pipeline.hpp"

namespace vierkant {

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
     * @brief   Retrieve a pipeline from the cache. Will create and cache a new pipeline, if necessary.
     *
     * @param   format  a Pipeline::Format describing the requested pipeline
     * @return  a const ref to a shared vierkant::Pipeline
     */
    const PipelinePtr &get(const Pipeline::Format &format)
    {
        // read-only locked for searching
        std::unordered_map<Pipeline::Format, PipelinePtr>::const_iterator it;
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            it = m_pipelines.find(format);
        }

        // found
        if(it != m_pipelines.end()){ return it->second; }
        else
        {
            // not found -> create pipeline
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            auto new_pipeline = Pipeline::create(m_device, format);
            auto pipe_it = m_pipelines.insert(std::make_pair(format, std::move(new_pipeline))).first;
            return pipe_it->second;
        }
    }

    void clear()
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_pipelines.clear();
    }

private:

    explicit PipelineCache(vierkant::DevicePtr device) : m_device(std::move(device)) {}

    vierkant::DevicePtr m_device;

    std::shared_mutex m_mutex;

    std::unordered_map<Pipeline::Format, PipelinePtr> m_pipelines;
};
}