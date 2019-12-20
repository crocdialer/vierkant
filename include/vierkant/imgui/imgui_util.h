#pragma once

#include <crocore/Component.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/imgui/imgui_integration.h>
#include <crocore/Application.hpp>

namespace vierkant::gui
{

//! draw a generic kinski::Component
void draw_component_ui(const crocore::ComponentConstPtr &the_component);

void draw_application_ui(const crocore::ApplicationPtr &app, const vierkant::WindowPtr &window);

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images);

}// namespaces

