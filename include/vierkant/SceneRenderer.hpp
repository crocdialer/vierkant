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
    virtual uint32_t render_scene(const vierkant::SceneConstPtr &scene, const CameraPtr &cam,
                                  const std::set<std::string> &tags) = 0;
};

}