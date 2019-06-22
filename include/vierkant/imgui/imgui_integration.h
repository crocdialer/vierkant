#include "vierkant/Application.hpp"

#include "imgui.h"
#include "ImGuizmo.h"

namespace kinski{ namespace gui {

bool init(kinski::App *the_app);

void shutdown();

void new_frame();

void end_frame();

void render();

void invalidate_device_objects();

bool create_device_objects();

void mouse_press(const vk::MouseEvent &e);
void mouse_wheel(const vk::MouseEvent &e);

void key_press(const vk::KeyEvent &e);
void key_release(const vk::KeyEvent &e);
void char_callback(uint32_t c);

}}// namespace