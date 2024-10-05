//
// Created by crocdialer on 6/7/20.
//

#pragma once

#include "vierkant/Camera.hpp"
#include "vierkant/Rasterizer.hpp"
#include "vierkant/Scene.hpp"
#include "vierkant/Semaphore.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(SceneRenderer)

class SceneRenderer
{
public:
    //! signature for a function. returns an id_entry_t for internal draw-indices
    using object_id_by_index_fn_t = std::function<vierkant::id_entry_t(uint32_t idx)>;

    //! groups results of rendering operations.
    struct render_result_t
    {
        uint32_t num_draws = 0;
        uint32_t num_frustum_culled = 0;
        uint32_t num_occlusion_culled = 0;
        uint32_t num_distance_culled = 0;
        vierkant::ImagePtr object_ids;
        object_id_by_index_fn_t object_by_index_fn;
        std::vector<semaphore_submit_info_t> semaphore_infos;
    };

    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scene       the scene to render.
     * @param   cam         the camera to use.
     * @param   tags        if not empty, only objects with at least one of the provided tags are rendered.
     * @return  a render_result_t object.
     */
    virtual render_result_t render_scene(vierkant::Rasterizer &renderer, const vierkant::SceneConstPtr &scene,
                                         const CameraPtr &cam, const std::set<std::string> &tags) = 0;

    virtual std::vector<uint16_t> pick(const glm::vec2 &normalized_coord) = 0;

    virtual ~SceneRenderer() = default;
};

}// namespace vierkant