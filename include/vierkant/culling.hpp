//
// Created by crocdialer on 6/14/20.
//

#pragma once

#include <unordered_set>

#include "vierkant/Scene.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/MeshNode.hpp"
#include "vierkant/Renderer.hpp"

namespace vierkant
{

//! a struct grouping drawables and other assets returned from a culling operation.
struct cull_result_t
{
    //! list of drawables
    std::vector<vierkant::drawable_t> drawables;

    std::unordered_set<vierkant::MeshConstPtr> meshes;

    std::unordered_set<vierkant::MeshNodeConstPtr> animated_nodes;

    //! reverse lookup: drawable -> scenegraph-node
    std::unordered_map<vierkant::DrawableId, vierkant::MeshNodeConstPtr> node_map;

//    //! a list of lightsources in eye-coordinates
//    std::vector<vierkant::Renderer::lightsource_t> lightsources;

    //! the camera used to perform culling
    CameraPtr camera;

    vierkant::SceneConstPtr scene;
};

struct cull_params_t
{
    vierkant::SceneConstPtr scene;
    CameraPtr camera;
    bool check_intersection = true;
    bool world_space = false;
    std::set<std::string> tags;
};

/**
 * @brief   Applies view-frustum culling for provided scene and camera.
 *
 * @param   scene       a provided scene.
 * @param   cull_params a struct grouping all parameters
 *
 * @return  a cull_result_t struct.
 */
cull_result_t cull(const cull_params_t &cull_params);

}
