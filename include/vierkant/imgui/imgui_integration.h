#include "vierkant/Window.hpp"
#include "vierkant/Renderer.hpp"
#include "imgui.h"
#include "ImGuizmo.h"

namespace vierkant::gui {

bool init(vierkant::WindowPtr w);

void shutdown();

void render(vierkant::Renderer &renderer);

void new_frame(const glm::vec2 &size, float delta_time);

//void end_frame();

}// namespace