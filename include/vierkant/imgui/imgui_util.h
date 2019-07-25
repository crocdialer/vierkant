#pragma once

#include <crocore/Component.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/imgui/imgui_integration.h>
#include <vierkant/Application.hpp>

namespace vierkant::gui {

//! draw a generic kinski::Component
void draw_component_ui(const crocore::ComponentConstPtr &the_component);

void draw_application_ui(const vierkant::ApplicationConstPtr &app);

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images);

}// namespaces

