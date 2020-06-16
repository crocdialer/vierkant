//
// Created by crocdialer on 6/15/20.
//

#pragma once

#include "vierkant/PipelineCache.hpp"
#include "vierkant/SceneRenderer.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(ForwardSceneRenderer);

class ForwardSceneRenderer : public vierkant::SceneRenderer
{
public:

    static ForwardSceneRendererPtr create(const vierkant::DevicePtr& device);

    ForwardSceneRenderer(const ForwardSceneRenderer &) = delete;

    ForwardSceneRenderer(ForwardSceneRenderer &&) = delete;

    ForwardSceneRenderer &operator=(ForwardSceneRenderer other) = delete;

    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scene       the scene to render.
     * @param   cam         the camera to use.
     * @param   tags        if not empty, only objects with at least one of the provided tags are rendered.
     * @return  the number of objects drawn
     */
    uint32_t render_scene(vierkant::Renderer &renderer,
                          const vierkant::SceneConstPtr &scene,
                          const CameraPtr &cam,
                          const std::set<std::string> &tags) override;

private:

    explicit ForwardSceneRenderer(const vierkant::DevicePtr& device);

    vierkant::PipelineCachePtr m_pipeline_cache;
};

}

