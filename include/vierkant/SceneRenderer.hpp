//
// Created by crocdialer on 6/7/20.
//

#pragma once

#include "vierkant/Semaphore.hpp"
#include "vierkant/Scene.hpp"
#include "vierkant/Camera.hpp"
#include "vierkant/Renderer.hpp"
#include "vierkant/DepthOfField.h"

namespace vierkant
{

DEFINE_CLASS_PTR(SceneRenderer);

class SceneRenderer
{
public:

    struct settings_t
    {
        //! disable colors from textures, material, vertices
        bool disable_material = false;

        //! draw the skybox, if any
        bool draw_skybox = true;

        //! draw a grid for orientation
        bool draw_grid = true;

        //! apply anti-aliasing using fxaa
        bool use_fxaa = true;

        //! bloom settings
        bool use_bloom = true;

        //! gamma correction of output
        float gamma = 1.0;

        //! exposure setting for tone-mapping
        float exposure = 2.0;

        //! desired depth-of-field settings, disabled by default
        postfx::dof_settings_t dof = {};
    };

    struct render_result_t
    {
        uint32_t num_objects = 0;
        std::vector<semaphore_submit_info_t> semaphore_infos;
    };

    //! settings struct
    settings_t settings;

    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scene       the scene to render.
     * @param   cam         the camera to use.
     * @param   tags        if not empty, only objects with at least one of the provided tags are rendered.
     * @return  a render_result_t object.
     */
    virtual render_result_t render_scene(vierkant::Renderer &renderer,
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