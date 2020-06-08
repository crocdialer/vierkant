//
// Created by crocdialer on 6/7/20.
//

#pragma once

#include "vierkant/Scene.hpp"
#include "vierkant/Camera.hpp"

namespace vierkant
{

class SceneRenderer
{
    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   scene   the scene to render.
     * @param   cam     the camera to use.
     * @param   tags    if not empty, only objects with at least one of the provided tags are rendered.
     * @return  the number of objects drawn
     */
    virtual uint32_t render_scene(const vierkant::SceneConstPtr &scene, const CameraPtr &cam,
                                  const std::set<std::string> &tags) = 0;
};

}