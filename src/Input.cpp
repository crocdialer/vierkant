#include <algorithm>
#include <array>
#include <crocore/crocore.hpp>
#include <stdexcept>
#include <vierkant/Input.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace vierkant
{

std::string to_string(Joystick::Input input)
{
    switch(input)
    {
        case Joystick::Input::ANALOG_LEFT_X: return "analog_left_x";
        case Joystick::Input::ANALOG_LEFT_Y: return "analog_left_y";
        case Joystick::Input::ANALOG_RIGHT_X: return "analog_right_x";
        case Joystick::Input::ANALOG_RIGHT_Y: return "analog_right_y";
        case Joystick::Input::DPAD_UP: return "dpad_up";
        case Joystick::Input::DPAD_DOWN: return "dpad_down";
        case Joystick::Input::DPAD_LEFT: return "dpad_left";
        case Joystick::Input::DPAD_RIGHT: return "dpad_right";
        case Joystick::Input::TRIGGER_LEFT: return "trigger_left";
        case Joystick::Input::TRIGGER_RIGHT: return "trigger_right";
        case Joystick::Input::BUTTON_A: return "button_a";
        case Joystick::Input::BUTTON_B: return "button_b";
        case Joystick::Input::BUTTON_X: return "button_x";
        case Joystick::Input::BUTTON_Y: return "button_y";
        case Joystick::Input::BUTTON_MENU: return "button_menu";
        case Joystick::Input::BUTTON_BACK: return "button_back";
        case Joystick::Input::BUTTON_BUMPER_LEFT: return "bumper_left";
        case Joystick::Input::BUTTON_BUMPER_RIGHT: return "bumper_right";
        case Joystick::Input::BUTTON_STICK_LEFT: return "stick_left";
        case Joystick::Input::BUTTON_STICK_RIGHT: return "stick_right";
        case Joystick::Input::BUTTON_GUIDE: return "button_guide";
    }
    throw std::runtime_error("missing case in to_string()");
}

//! canonical button-order, matching GLFW_GAMEPAD_BUTTON_*. total, so no index can alias.
constexpr std::array<Joystick::Input, GLFW_GAMEPAD_BUTTON_LAST + 1> g_button_inputs = {
        Joystick::Input::BUTTON_A,          Joystick::Input::BUTTON_B,           Joystick::Input::BUTTON_X,
        Joystick::Input::BUTTON_Y,          Joystick::Input::BUTTON_BUMPER_LEFT, Joystick::Input::BUTTON_BUMPER_RIGHT,
        Joystick::Input::BUTTON_BACK,       Joystick::Input::BUTTON_MENU,        Joystick::Input::BUTTON_GUIDE,
        Joystick::Input::BUTTON_STICK_LEFT, Joystick::Input::BUTTON_STICK_RIGHT, Joystick::Input::DPAD_UP,
        Joystick::Input::DPAD_RIGHT,        Joystick::Input::DPAD_DOWN,          Joystick::Input::DPAD_LEFT};

//! shared by analog_left/analog_right: radial sign, smoothstep dead-zone per axis
static glm::vec2 analog_value(const std::vector<float> &axis, uint32_t index_h, uint32_t index_v, float dead_zone)
{
    if(index_h >= axis.size() || index_v >= axis.size()) { return {}; }
    auto sign_h = static_cast<float>(crocore::sgn(axis[index_h]));
    auto sign_v = static_cast<float>(crocore::sgn(axis[index_v]));

    return {sign_h * glm::smoothstep(dead_zone, 1.f, fabsf(axis[index_h])),
            sign_v * glm::smoothstep(dead_zone, 1.f, fabsf(axis[index_v]))};
}

Joystick::Joystick(std::string name, std::vector<uint8_t> buttons, std::vector<float> axis,
                   const std::vector<uint8_t> &previous_buttons, rumble_fn_t rumble_fn)
    : m_name(std::move(name)), m_buttons(std::move(buttons)), m_axis(std::move(axis)),
      m_rumble_fn(std::move(rumble_fn))
{
    if(m_buttons.size() == previous_buttons.size())
    {
        for(uint32_t i = 0; i < std::min(m_buttons.size(), g_button_inputs.size()); ++i)
        {
            if(m_buttons[i] != previous_buttons[i])
            {
                const Joystick::Input js_input = g_button_inputs[i];
                m_input_events[js_input] = m_buttons[i] ? Event::BUTTON_PRESS : Event::BUTTON_RELEASE;
                spdlog::trace("{}: {}", to_string(js_input), m_buttons[i] ? "press" : "release");
            }
        }
    }
}

const std::string &Joystick::name() const { return m_name; }

const std::vector<uint8_t> &Joystick::buttons() const { return m_buttons; }

const std::vector<float> &Joystick::axis() const { return m_axis; }

glm::vec2 Joystick::analog_left() const
{ return analog_value(m_axis, GLFW_GAMEPAD_AXIS_LEFT_X, GLFW_GAMEPAD_AXIS_LEFT_Y, dead_zone); }

glm::vec2 Joystick::analog_right() const
{ return analog_value(m_axis, GLFW_GAMEPAD_AXIS_RIGHT_X, GLFW_GAMEPAD_AXIS_RIGHT_Y, dead_zone); }

glm::vec2 Joystick::trigger() const
{
    constexpr uint32_t index_l = GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, index_r = GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
    if(index_l >= m_axis.size() || index_r >= m_axis.size()) { return {}; }
    return (glm::vec2(fabs(m_axis[index_l]) > dead_zone ? m_axis[index_l] : 0.f,
                      fabs(m_axis[index_r]) > dead_zone ? m_axis[index_r] : 0.f) +
            1.f) /
           2.f;
}

glm::vec2 Joystick::dpad() const
{
    auto val_fn = [this](uint32_t button_idx) -> float {
        return button_idx < m_buttons.size() && m_buttons[button_idx] ? 1.f : 0.f;
    };
    return {val_fn(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT) - val_fn(GLFW_GAMEPAD_BUTTON_DPAD_LEFT),
            val_fn(GLFW_GAMEPAD_BUTTON_DPAD_UP) - val_fn(GLFW_GAMEPAD_BUTTON_DPAD_DOWN)};
}

const std::unordered_map<Joystick::Input, Joystick::Event> &Joystick::input_events() const { return m_input_events; }

bool Joystick::rumble(float strong, float weak, uint32_t duration_ms) const
{
    return m_rumble_fn && m_rumble_fn(strong, weak, duration_ms);
}

}// namespace vierkant
