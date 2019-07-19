#pragma once

#include <crocore/Component.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/imgui/imgui_integration.h>

namespace vierkant::gui{

//! draw a generic kinski::Component using ImGui
void draw_component_ui(const crocore::ComponentConstPtr &the_component);


}// namespaces

