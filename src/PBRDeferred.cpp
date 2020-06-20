//
// Created by crocdialer on 6/19/20.
//

#include "vierkant/PBRDeferred.hpp"

namespace vierkant
{

uint32_t PBRDeferred::render_scene(Renderer &renderer, const SceneConstPtr &scene, const CameraPtr &cam,
                                   const std::set<std::string> &tags)
{
    return 0;
}

PBRDeferred::PBRDeferred(const DevicePtr &device):m_pipeline_cache(vierkant::PipelineCache::create(device))
{

}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device));
}

}// namespace vierkant