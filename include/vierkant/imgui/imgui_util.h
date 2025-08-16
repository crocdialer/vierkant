#pragma once

#include <deque>

#include <crocore/Application.hpp>

#include <vierkant/Image.hpp>
#include <vierkant/SceneRenderer.hpp>
#include <vierkant/Window.hpp>
#include <vierkant/imgui/imgui_integration.h>

namespace vierkant::gui
{

void draw_application_ui(const crocore::ApplicationPtr &app, const vierkant::WindowPtr &window);

void draw_logger_ui(const std::deque<std::pair<std::string, spdlog::level::level_enum>> &items);

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images);

void draw_scene_ui(const vierkant::ScenePtr &scene, CameraPtr &cam,
                   std::set<vierkant::Object3DPtr> *selection = nullptr);

void draw_scene_renderer_settings_ui(const vierkant::SceneRendererPtr &scene_renderer);

void draw_scene_renderer_statistics_ui(const vierkant::SceneRendererPtr &scene_renderer);

void draw_object_ui(const vierkant::Object3DPtr &object);

void draw_camera_param_ui(vierkant::physical_camera_params_t &camera_params);

enum class GuizmoType
{
    INACTIVE = 0,
    TRANSLATE,
    ROTATE,
    SCALE
};

bool draw_transform_guizmo(vierkant::transform_t &transform, const vierkant::CameraConstPtr &camera, GuizmoType type);

void draw_transform_guizmo(const vierkant::Object3DPtr &object, const vierkant::CameraConstPtr &camera,
                           GuizmoType type);

void draw_transform_guizmo(const std::set<vierkant::Object3DPtr> &object_set, const vierkant::CameraConstPtr &camera,
                           GuizmoType type);

bool draw_material_ui(vierkant::material_t &material,
                      const std::function<void(vierkant::TextureType, const std::string &)> &draw_texture_fn = {});

}// namespace vierkant::gui
