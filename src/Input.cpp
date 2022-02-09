#include "vierkant/Input.hpp"

namespace vierkant
{

JoystickState::ButtonMap JoystickState::s_button_map =
        {
                {Mapping::ANALOG_LEFT_H,       0},
                {Mapping::ANALOG_LEFT_V,       1},
                {Mapping::ANALOG_RIGHT_H,      3},
                {Mapping::ANALOG_RIGHT_V,      4},
                {Mapping::TRIGGER_LEFT,        2},
                {Mapping::TRIGGER_RIGHT,       5},
                {Mapping::DPAD_H,              6},
                {Mapping::DPAD_V,              7},

                {Mapping::BUTTON_A,            0},
                {Mapping::BUTTON_B,            1},
                {Mapping::BUTTON_X,            2},
                {Mapping::BUTTON_Y,            3},
                {Mapping::BUTTON_MENU,         7},
                {Mapping::BUTTON_BACK,         6},
                {Mapping::BUTTON_BUMPER_LEFT,  8},
                {Mapping::BUTTON_BUMPER_RIGHT, 9},
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

const std::string &JoystickState::name() const{ return m_name; };

const std::vector<uint8_t> &JoystickState::buttons() const{ return m_buttons; };

const std::vector<float> &JoystickState::axis() const{ return m_axis; };

glm::vec2 JoystickState::analog_left() const
{
    uint32_t index_h = s_button_map[Mapping::ANALOG_LEFT_H], index_v = s_button_map[Mapping::ANALOG_LEFT_V];
    auto sign_h = static_cast<float>(crocore::sgn(m_axis[index_h]));
    auto sign_v = static_cast<float>(crocore::sgn(m_axis[index_v]));

    return {sign_h * glm::smoothstep(m_dead_zone, 1.f, fabsf(m_axis[index_h])),
            sign_v * glm::smoothstep(m_dead_zone, 1.f, fabsf(m_axis[index_v]))};
}

glm::vec2 JoystickState::analog_right() const
{
    uint32_t index_h = s_button_map[Mapping::ANALOG_RIGHT_H], index_v = s_button_map[Mapping::ANALOG_RIGHT_V];
    auto sign_h = static_cast<float>(crocore::sgn(m_axis[index_h]));
    auto sign_v = static_cast<float>(crocore::sgn(m_axis[index_v]));

    return {sign_h * glm::smoothstep(m_dead_zone, 1.f, fabsf(m_axis[index_h])),
            sign_v * glm::smoothstep(m_dead_zone, 1.f, fabsf(m_axis[index_v]))};
}

glm::vec2 JoystickState::trigger() const
{
    uint32_t index_h = s_button_map[Mapping::TRIGGER_LEFT], index_v = s_button_map[Mapping::TRIGGER_RIGHT];
    return {fabs(m_axis[index_h]) > m_dead_zone ? m_axis[index_h] : 0.f,
            fabs(m_axis[index_v]) > m_dead_zone ? m_axis[index_v] : 0.f};
}

glm::vec2 JoystickState::dpad() const
{
    uint32_t index_h = s_button_map[Mapping::DPAD_H], index_v = s_button_map[Mapping::DPAD_V];
    return {fabs(m_axis[index_h]) > m_dead_zone ? m_axis[index_h] : 0.f,
            fabs(m_axis[index_v]) > m_dead_zone ? m_axis[index_v] : 0.f};
}
}
