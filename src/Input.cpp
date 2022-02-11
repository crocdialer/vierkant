#include "vierkant/Input.hpp"

namespace vierkant
{

std::string to_string(Joystick::Input input)
{
    switch(input)
    {
        case Joystick::Input::ANALOG_LEFT_X:
            return "analog_left_x";
        case Joystick::Input::ANALOG_LEFT_Y:
            return "analog_left_y";
        case Joystick::Input::ANALOG_RIGHT_X:
            return "analog_right_x";
        case Joystick::Input::ANALOG_RIGHT_Y:
            return "analog_right_y";
        case Joystick::Input::DPAD_X:
            return "dpad_x";
        case Joystick::Input::DPAD_Y:
            return "dpad_y";
        case Joystick::Input::TRIGGER_LEFT:
            return "trigger_left";
        case Joystick::Input::TRIGGER_RIGHT:
            return "trigger_right";
        case Joystick::Input::BUTTON_A:
            return "button_a";
        case Joystick::Input::BUTTON_B:
            return "button_b";
        case Joystick::Input::BUTTON_X:
            return "button_x";
        case Joystick::Input::BUTTON_Y:
            return "button_y";
        case Joystick::Input::BUTTON_MENU:
            return "button_menu";
        case Joystick::Input::BUTTON_BACK:
            return "button_back";
        case Joystick::Input::BUTTON_BUMPER_LEFT:
            return "bumper_left";
        case Joystick::Input::BUTTON_BUMPER_RIGHT:
            return "bumper_right";
        case Joystick::Input::BUTTON_STICK_LEFT:
            return "stick_left";
        case Joystick::Input::BUTTON_STICK_RIGHT:
            return "stick_right";
    }
    throw std::runtime_error("missing case in to_string()");
}

struct mapping_t
{
    std::unordered_map <Joystick::Input, uint32_t> input_to_axis;
    std::unordered_map <Joystick::Input, uint32_t> input_to_button;
    std::unordered_map <uint32_t, Joystick::Input> button_to_input;

    mapping_t()
    {
        input_to_axis =
                {
                {Joystick::Input::ANALOG_LEFT_X,       0},
                {Joystick::Input::ANALOG_LEFT_Y,       1},
                {Joystick::Input::ANALOG_RIGHT_X,      3},
                {Joystick::Input::ANALOG_RIGHT_Y,      4},
                {Joystick::Input::TRIGGER_LEFT,        2},
                {Joystick::Input::TRIGGER_RIGHT,       5},
                {Joystick::Input::DPAD_X,              6},
                {Joystick::Input::DPAD_Y,              7},

                };
        input_to_button =
                {
                        {Joystick::Input::BUTTON_A,            0},
                        {Joystick::Input::BUTTON_B,            1},
                        {Joystick::Input::BUTTON_X,            2},
                        {Joystick::Input::BUTTON_Y,            3},
                        {Joystick::Input::BUTTON_BUMPER_LEFT,  4},
                        {Joystick::Input::BUTTON_BUMPER_RIGHT, 5},
                        {Joystick::Input::BUTTON_BACK,         6},
                        {Joystick::Input::BUTTON_MENU,         7},
                        {Joystick::Input::BUTTON_STICK_LEFT,   8},
                        {Joystick::Input::BUTTON_STICK_RIGHT,  9}
                };

        for(const auto &[input, button] : input_to_button){ button_to_input[button] = input; }
    }
};

mapping_t g_mapping;

Joystick::Joystick(std::string name,
                   std::vector <uint8_t> buttons,
                   std::vector<float> axis,
                   const std::vector <uint8_t> &previous_buttons) :
        m_name(std::move(name)),
        m_buttons(std::move(buttons)),
        m_axis(std::move(axis))
{
    if(m_buttons.size() == previous_buttons.size())
    {
        for(uint32_t i = 0; i < m_buttons.size(); ++i)
        {
            if(m_buttons[i] != previous_buttons[i])
            {
                m_input_events[g_mapping.button_to_input[i]] = m_buttons[i] ? Event::BUTTON_PRESS
                                                                            : Event::BUTTON_RELEASE;
            }
        }
    }
}

const std::string &Joystick::name() const{ return m_name; }

const std::vector <uint8_t> &Joystick::buttons() const{ return m_buttons; }

const std::vector<float> &Joystick::axis() const{ return m_axis; }

glm::vec2 Joystick::analog_left() const
{
    uint32_t index_h = g_mapping.input_to_axis[Input::ANALOG_LEFT_X], index_v = g_mapping.input_to_axis[Input::ANALOG_LEFT_Y];
    auto sign_h = static_cast<float>(crocore::sgn(m_axis[index_h]));
    auto sign_v = static_cast<float>(crocore::sgn(m_axis[index_v]));

    return {sign_h * glm::smoothstep(dead_zone, 1.f, fabsf(m_axis[index_h])),
            sign_v * glm::smoothstep(dead_zone, 1.f, fabsf(m_axis[index_v]))};
}

glm::vec2 Joystick::analog_right() const
{
    uint32_t index_h = g_mapping.input_to_axis[Input::ANALOG_RIGHT_X], index_v = g_mapping.input_to_axis[Input::ANALOG_RIGHT_Y];
    auto sign_h = static_cast<float>(crocore::sgn(m_axis[index_h]));
    auto sign_v = static_cast<float>(crocore::sgn(m_axis[index_v]));

    return {sign_h * glm::smoothstep(dead_zone, 1.f, fabsf(m_axis[index_h])),
            sign_v * glm::smoothstep(dead_zone, 1.f, fabsf(m_axis[index_v]))};
}

glm::vec2 Joystick::trigger() const
{
    uint32_t index_l = g_mapping.input_to_axis[Input::TRIGGER_LEFT], index_r = g_mapping.input_to_axis[Input::TRIGGER_RIGHT];
    return (glm::vec2(fabs(m_axis[index_l]) > dead_zone ? m_axis[index_l] : 0.f,
                      fabs(m_axis[index_r]) > dead_zone ? m_axis[index_r] : 0.f) + 1.f) / 2.f;
}

glm::vec2 Joystick::dpad() const
{
    uint32_t index_h = g_mapping.input_to_axis[Input::DPAD_X], index_v = g_mapping.input_to_axis[Input::DPAD_Y];
    return {fabs(m_axis[index_h]) > dead_zone ? m_axis[index_h] : 0.f,
            fabs(m_axis[index_v]) > dead_zone ? m_axis[index_v] : 0.f};
}

const std::unordered_map <Joystick::Input, Joystick::Event> &Joystick::input_events() const
{
    return m_input_events;
}

}
