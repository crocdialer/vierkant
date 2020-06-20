//
// Created by crocdialer on 6/19/20.
//

#include "vierkant/culling.hpp"
#include "vierkant/PBRDeferred.hpp"

namespace vierkant
{

uint32_t PBRDeferred::render_scene(Renderer &renderer, const SceneConstPtr &scene, const CameraPtr &cam,
                                   const std::set<std::string> &tags)
{
    auto cull_result = vierkant::cull(scene, cam, true, tags);
    uint32_t num_drawables = cull_result.drawables.size();

    // create g-buffer
    // lighting-pass
    // dof, bloom
    // skybox
    // compositing, post-fx
    return num_drawables;
}

PBRDeferred::PBRDeferred(const DevicePtr &device):m_pipeline_cache(vierkant::PipelineCache::create(device))
{

}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device));
}

}// namespace vierkant