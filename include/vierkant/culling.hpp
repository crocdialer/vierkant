//
// Created by crocdialer on 6/14/20.
//

#pragma once

#include <unordered_set>

#include "vierkant/Camera.hpp"
#include "vierkant/Rasterizer.hpp"
#include "vierkant/Scene.hpp"
#include "vierkant/punctual_light.hpp"

namespace vierkant
{

using matrix_cache_t = std::unordered_map<id_entry_t, vierkant::matrix_struct_t>;
using index_cache_t = std::unordered_map<id_entry_t, uint32_t>;

//! a struct grouping drawables and other assets returned from a culling operation.
struct cull_result_t
{
    //! list of drawables
    std::vector<vierkant::drawable_t> drawables;

    std::unordered_set<vierkant::MeshConstPtr> meshes;

    //! list of light-sources
    std::vector<vierkant::lightsource_ubo_t> lights;

    //! lookup: drawable-id -> entity/entry
    std::unordered_map<vierkant::DrawableId, id_entry_t> entity_map;

    //! lookup: (id/entry) -> drawable-index
    index_cache_t index_map;

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
 * @param   cull_params a struct grouping all parameters*
 *
 * @return  a cull_result_t struct.
 */
cull_result_t cull(const cull_params_t &cull_params);

}
