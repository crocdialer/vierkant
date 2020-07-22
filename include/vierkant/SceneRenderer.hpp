//
// Created by crocdialer on 6/7/20.
//

#pragma once

#include "vierkant/Scene.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/Renderer.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(SceneRenderer);

class SceneRenderer
{
public:

    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scene       the scene to render.
     * @param   cam         the camera to use.
     * @param   tags        if not empty, only objects with at least one of the provided tags are rendered.
     * @return  the number of objects drawn
     */
    virtual uint32_t render_scene(vierkant::Renderer &renderer,
                                  const vierkant::SceneConstPtr &scene,
                                  const CameraPtr &cam,
                                  const std::set<std::string> &tags) = 0;

    /**
     * @brief   Set an environment-cubemap.
     *
     * @param   cubemap     an environment-cubemap.
     */
    virtual void set_environment(const vierkant::ImagePtr &cubemap) = 0;
};

}