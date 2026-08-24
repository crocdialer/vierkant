//  MIT License
//
//  Copyright (c) 2023 Fabian Schmidt (github.com/crocdialer)
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.

#pragma once

#include <crocore/Application.hpp>

#include <filesystem>
#include <vierkant/Scene.hpp>
#include <vierkant/SceneRenderer.hpp>

class PBRThumbnailer : public crocore::Application
{
public:
    struct settings_t
    {
        //! desired log-level
        spdlog::level::level_enum log_level = spdlog::level::off;

        //! path to an input model-file (.gltf | .glb)
        std::filesystem::path model_path;

        //! optional path to an input HDR environment-map (.hdr)
        std::filesystem::path environment_path;

        //! output-image path
        std::filesystem::path result_image_path;

        //! output-image resolution
        glm::uvec2 result_image_size = {1024, 1024};

        //! elevation- and azimuth-angles for camera-placement in radians
        glm::vec2 cam_spherical_coords = {glm::radians(-28.6479f), glm::radians(20.f)};

        //! flag to request a path-tracer rendering-backend
        bool use_pathtracer = true;

        //! required total number of samples-per-pixel (spp) (applies only to path-tracer)
        uint32_t num_samples = 1024;

        //! maximum number of samples-per-pixel (spp), per frame (applies only to path-tracer)
        uint32_t max_samples_per_frame = 8;

        //! maximum path-length (applies only to path-tracer)
        uint32_t max_path_length = 8;

        //! random-seed to initialize path-tracer RNG
        uint32_t random_seed = 0;

        //! flag to request drawing of used skybox
        bool draw_skybox = false;

        //! flag to use a camera contained in the model/scene file, if any
        bool use_model_camera = false;

        //! flag to enable vulkan validation-layers
        bool use_validation = false;
    };

    explicit PBRThumbnailer(const crocore::Application::create_info_t &create_info, settings_t settings)
        : crocore::Application(create_info), m_context(), m_settings(std::move(settings)) {};

private:
    struct graphics_context_t
    {
        // instance
        vierkant::Instance instance;

        // device
        vierkant::DevicePtr device;

        // output rasterizer
        vierkant::Rasterizer renderer;
        vierkant::Framebuffer framebuffer;

        vierkant::SceneRendererPtr scene_renderer = nullptr;
    };

    static std::optional<vierkant::model::model_assets_t> load_model_file(const std::filesystem::path &path,
                                                                          crocore::ThreadPoolClassic &pool);

    void setup() override;

    void update(double time_delta) override;

    void teardown() override;

    void poll_events() override {};

    bool create_graphics_context();

    vierkant::AABB create_mesh(const vierkant::model::model_assets_t &mesh_assets);

    void create_camera(const vierkant::model::model_assets_t &mesh_assets, const vierkant::AABB &model_aabb);

    graphics_context_t m_context;

    vierkant::ScenePtr m_scene = vierkant::Scene::create();

    vierkant::Object3DPtr m_camera;

    settings_t m_settings;
};
