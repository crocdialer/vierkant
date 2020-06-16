//
// Created by crocdialer on 6/14/20.
//

#pragma once

#include "vierkant/Scene.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/Renderer.hpp"

namespace vierkant
{

//! a struct grouping drawables and other assets returned from a culling operation.
struct cull_result_t
{
    //! a list of drawables in eye-coordinates
    std::vector<vierkant::Renderer::drawable_t> drawables;

    //! a list of lightsources in eye-coordinates
    std::vector<vierkant::Renderer::lightsource_t> lightsources;
};

/**
 * @brief   Applies view-frustum culling for provided scene and camera.
 * @param   scene   a provided scene.
 * @param   cam     a provided camera.
 * @return  a cull_result_t struct.
 */
cull_result_t cull(const vierkant::SceneConstPtr &scene, const CameraPtr &cam, const std::set<std::string> &tags = {});

}
