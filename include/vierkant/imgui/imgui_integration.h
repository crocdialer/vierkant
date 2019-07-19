#include "vierkant/Window.hpp"
#include "vierkant/Renderer.hpp"
#include "imgui.h"
#include "ImGuizmo.h"

namespace vierkant::gui {

bool init(vierkant::WindowPtr w);

void shutdown();

void new_frame();

void end_frame();

void render(vierkant::Renderer &renderer);

void invalidate_device_objects();

bool create_device_objects(vierkant::DevicePtr device);

void mouse_press(const vierkant::MouseEvent &e);

void mouse_wheel(const vierkant::MouseEvent &e);

void key_press(const vierkant::KeyEvent &e);

void key_release(const vierkant::KeyEvent &e);

void char_callback(uint32_t c);

}// namespace