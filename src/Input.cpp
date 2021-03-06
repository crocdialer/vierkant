#include "vierkant/Input.hpp"

#include <utility>

namespace vierkant {

std::vector<JoystickState> get_joystick_states()
{
    std::vector<JoystickState> ret;
    int count;
    for(int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++)
    {
        if(!glfwJoystickPresent(i)) continue;

        const float *glfw_axis = glfwGetJoystickAxes(i, &count);
        std::vector<float> axis(glfw_axis, glfw_axis + count);

        const uint8_t *glfw_buttons = glfwGetJoystickButtons(i, &count);
        std::vector<uint8_t> buttons(glfw_buttons, glfw_buttons + count);

        std::string name(glfwGetJoystickName(i));
        ret.emplace_back(name, buttons, axis);
    }
    return ret;
}

JoystickState::ButtonMap JoystickState::s_button_map =
        {
                {Mapping::ANALOG_LEFT_H,       0},
                {Mapping::ANALOG_LEFT_V,       1},
                {Mapping::ANALOG_RIGHT_H,      2},
                {Mapping::ANALOG_RIGHT_V,      3},
                {Mapping::TRIGGER_LEFT,        4},
                {Mapping::TRIGGER_RIGHT,       5},
                {Mapping::DPAD_H,              6},
                {Mapping::DPAD_V,              7},

                {Mapping::BUTTON_A,            0},
                {Mapping::BUTTON_B,            1},
                {Mapping::BUTTON_X,            3},
                {Mapping::BUTTON_Y,            4},
                {Mapping::BUTTON_MENU,         11},
                {Mapping::BUTTON_BACK,         11},
                {Mapping::BUTTON_BUMPER_LEFT,  6},
                {Mapping::BUTTON_BUMPER_RIGHT, 7},
                {Mapping::BUTTON_STICK_LEFT,   13},
                {Mapping::BUTTON_STICK_RIGHT,  14}
        };

JoystickState::JoystickState(std::string n,
                             std::vector<uint8_t> b,
                             std::vector<float> a) :
        m_name(std::move(n)),
        m_buttons(std::move(b)),
        m_axis(std::move(a))
{

}

const std::string &JoystickState::name() const { return m_name; };

const std::vector<uint8_t> &JoystickState::buttons() const { return m_buttons; };

const std::vector<float> &JoystickState::axis() const { return m_axis; };

glm::vec2 JoystickState::analog_left() const
{
    uint32_t index_h = s_button_map[Mapping::ANALOG_LEFT_H], index_v = s_button_map[Mapping::ANALOG_LEFT_V];
    return glm::vec2(fabs(m_axis[index_h]) > m_dead_zone ? m_axis[index_h] : 0.f,
                     fabs(m_axis[index_v]) > m_dead_zone ? m_axis[index_v] : 0.f);
}

glm::vec2 JoystickState::analog_right() const
{
    uint32_t index_h = s_button_map[Mapping::ANALOG_RIGHT_H], index_v = s_button_map[Mapping::ANALOG_RIGHT_V];
    return glm::vec2(fabs(m_axis[index_h]) > m_dead_zone ? m_axis[index_h] : 0.f,
                     fabs(m_axis[index_v]) > m_dead_zone ? m_axis[index_v] : 0.f);
}

glm::vec2 JoystickState::trigger() const
{
    uint32_t index_h = s_button_map[Mapping::TRIGGER_LEFT], index_v = s_button_map[Mapping::TRIGGER_RIGHT];
    return glm::vec2(fabs(m_axis[index_h]) > m_dead_zone ? m_axis[index_h] : 0.f,
                     fabs(m_axis[index_v]) > m_dead_zone ? m_axis[index_v] : 0.f);
}

glm::vec2 JoystickState::dpad() const
{
    uint32_t index_h = s_button_map[Mapping::DPAD_H], index_v = s_button_map[Mapping::DPAD_V];
    return glm::vec2(fabs(m_axis[index_h]) > m_dead_zone ? m_axis[index_h] : 0.f,
                     fabs(m_axis[index_v]) > m_dead_zone ? m_axis[index_v] : 0.f);
}
}
