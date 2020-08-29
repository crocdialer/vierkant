#pragma once

#include <crocore/Application.hpp>
#include <crocore/Component.hpp>

#include <vierkant/Image.hpp>
#include <vierkant/Window.hpp>
#include <vierkant/SceneRenderer.hpp>
#include <vierkant/imgui/imgui_integration.h>

namespace vierkant::gui
{

//! draw a generic crocore::Component
void draw_component_ui(const crocore::ComponentConstPtr &the_component);

void draw_application_ui(const crocore::ApplicationPtr &app, const vierkant::WindowPtr &window);

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images);

void draw_scene_ui(const vierkant::SceneConstPtr &scene,
                   const vierkant::CameraConstPtr &camera = nullptr,
                   std::set<vierkant::Object3DPtr> *selection = nullptr);

void draw_scene_renderer_ui(const vierkant::SceneRendererPtr &scene_renderer);

void draw_object_ui(const vierkant::Object3DPtr &object, const vierkant::CameraConstPtr &camera = nullptr);

}// namespaces

