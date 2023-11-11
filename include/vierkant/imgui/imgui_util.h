#pragma once

#include <deque>

#include <crocore/Application.hpp>

#include <vierkant/Image.hpp>
#include <vierkant/Window.hpp>
#include <vierkant/SceneRenderer.hpp>
#include <vierkant/imgui/imgui_integration.h>

namespace vierkant::gui
{

void draw_application_ui(const crocore::ApplicationPtr &app, const vierkant::WindowPtr &window);

void draw_logger_ui(const std::deque<std::pair<std::string, spdlog::level::level_enum>> &items);

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images);

void draw_scene_ui(const vierkant::ScenePtr &scene,
                   CameraPtr &cam,
                   std::set<vierkant::Object3DPtr> *selection = nullptr);

void draw_scene_renderer_ui(const vierkant::SceneRendererPtr &scene_renderer);

void draw_object_ui(const vierkant::Object3DPtr &object);

void draw_camera_param_ui(vierkant::physical_camera_component_t &camera_params);

enum class GuizmoType
{
    INACTIVE = 0,
    TRANSLATE,
    ROTATE,
    SCALE
};

void draw_transform_guizmo(const vierkant::Object3DPtr &object,
                           const vierkant::CameraConstPtr &camera,
                           GuizmoType type);

}// namespaces

