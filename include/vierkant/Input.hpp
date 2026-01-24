#pragma once

#include <string>
#include <unordered_map>
#include <vierkant/math.hpp>

namespace vierkant
{

class MouseEvent;

class KeyEvent;

class Joystick;

/**
 * @brief   mouse_delegate_t is a struct to group mouse-callbacks.
 */
struct mouse_delegate_t
{
    using enabled_cb_t = std::function<bool()>;
    using mouse_cb_t = std::function<void(const MouseEvent &)>;
    using file_drop_cb_t = std::function<void(const MouseEvent &, const std::vector<std::string> &)>;

    enabled_cb_t enabled;
    mouse_cb_t mouse_press;
    mouse_cb_t mouse_release;
    mouse_cb_t mouse_move;
    mouse_cb_t mouse_drag;
    mouse_cb_t mouse_wheel;
    file_drop_cb_t file_drop;
};

/**
 * @brief   key_delegate_t is a struct to group keyboard-callbacks.
 */
struct key_delegate_t
{
    using enabled_cb_t = std::function<bool()>;
    using key_cb_t = std::function<void(const KeyEvent &)>;
    using char_cb_t = std::function<void(uint32_t)>;

    enabled_cb_t enabled;
    key_cb_t key_press;
    key_cb_t key_release;
    char_cb_t character_input;
};

/**
 * @brief   joystick_delegate_t is a struct to group keyboard-callbacks
 */
struct joystick_delegate_t
{
    using enabled_cb_t = std::function<bool()>;
    using joystick_cb_t = std::function<void(const std::vector<Joystick> &states)>;

    enabled_cb_t enabled;
    joystick_cb_t joystick_cb;
};

//! Represents a mouse event
class MouseEvent
{
public:
    MouseEvent() = default;

    MouseEvent(int initiator, int x, int y, unsigned int modifiers, glm::vec2 wheel_inc)
        : m_initiator(initiator), m_x(x), m_y(y), m_modifiers(modifiers), m_wheel_inc(wheel_inc)
    {}

    //! Returns the X coordinate of the mouse event
    int get_x() const { return m_x; }

    //! Returns the Y coordinate of the mouse event
    int get_y() const { return m_y; }

    //! Returns the coordinates of the mouse event
    [[nodiscard]] glm::ivec2 position() const { return {m_x, m_y}; }

    //! Returns the number of detents the user has wheeled through. Positive values correspond to wheel-up and negative to wheel-down.
    [[nodiscard]] glm::vec2 wheel_increment() const { return m_wheel_inc; }

    //! Returns whether the initiator for the event was the left mouse button
    bool is_left() const { return m_initiator & BUTTON_LEFT; }

    //! Returns whether the initiator for the event was the right mouse button
    bool is_right() const { return m_initiator & BUTTON_RIGHT; }

    //! Returns whether the initiator for the event was the middle mouse button
    bool is_middle() const { return m_initiator & BUTTON_MIDDLE; }

    //! Returns whether the left mouse button was pressed during the event
    bool is_left_down() const { return m_modifiers & BUTTON_LEFT; }

    //! Returns whether the right mouse button was pressed during the event
    bool is_right_down() const { return m_modifiers & BUTTON_RIGHT; }

    //! Returns whether the middle mouse button was pressed during the event
    bool is_middle_down() const { return m_modifiers & BUTTON_MIDDLE; }

    //! Returns whether the Shift key was pressed during the event.
    bool is_shift_down() const { return m_modifiers & SHIFT_DOWN; }

    //! Returns whether the Alt (or Option) key was pressed during the event.
    bool is_alt_down() const { return m_modifiers & ALT_DOWN; }

    //! Returns whether the Control key was pressed during the event.
    bool is_control_down() const { return m_modifiers & CTRL_DOWN; }

    //! Returns whether the meta key was pressed during the event. Maps to the Windows key on Windows and the Command key on Mac OS X.
    bool is_meta_down() const { return m_modifiers & META_DOWN; }

    enum
    {
        BUTTON_LEFT = (1 << 0),
        BUTTON_RIGHT = (1 << 1),
        BUTTON_MIDDLE = (1 << 2),
        SHIFT_DOWN = (1 << 3),
        ALT_DOWN = (1 << 4),
        CTRL_DOWN = (1 << 5),
        META_DOWN = (1 << 6),
        TOUCH_DOWN = (1 << 7)
    };

private:
    uint32_t m_initiator = 0;
    int m_x = 0, m_y = 0;
    unsigned int m_modifiers = 0;
    glm::vec2 m_wheel_inc{0};
};

//! Represents a keyboard event
class KeyEvent
{
public:
    KeyEvent(int code, uint32_t character, uint32_t modifiers) : m_code(code), m_char(character), m_modifiers(modifiers)
    {}

    //! Returns the key code associated with the event (maps into Key::Type enum)
    int code() const { return m_code; }

    //! Returns the Unicode character associated with the event.
    uint32_t character() const { return m_char; }

    //! Returns whether the Shift key was pressed during the event.
    bool is_shift_down() const { return m_modifiers & SHIFT_DOWN; }

    //! Returns whether the Alt (or Option) key was pressed during the event.
    bool is_alt_down() const { return m_modifiers & ALT_DOWN; }

    //! Returns whether the Control key was pressed during the event.
    bool is_control_down() const { return m_modifiers & CTRL_DOWN; }

    //! Returns whether the meta key was pressed during the event. Maps to the Windows key on Windows and the Command key on Mac OS X.
    bool is_meta_down() const { return m_modifiers & META_DOWN; }

    enum
    {
        SHIFT_DOWN = (1 << 3),
        ALT_DOWN = (1 << 4),
        CTRL_DOWN = (1 << 5),
        META_DOWN = (1 << 6)
    };

private:
    int m_code = 0;
    uint32_t m_char = 0;
    uint32_t m_modifiers = 0;
};

class Joystick
{
public:
    enum class Event
    {
        BUTTON_PRESS,
        BUTTON_RELEASE
    };

    enum class Input : uint32_t
    {
        ANALOG_LEFT_X,
        ANALOG_LEFT_Y,
        ANALOG_RIGHT_X,
        ANALOG_RIGHT_Y,
        DPAD_X,
        DPAD_Y,
        TRIGGER_LEFT,
        TRIGGER_RIGHT,
        BUTTON_A,
        BUTTON_B,
        BUTTON_X,
        BUTTON_Y,
        BUTTON_MENU,
        BUTTON_BACK,
        BUTTON_BUMPER_LEFT,
        BUTTON_BUMPER_RIGHT,
        BUTTON_STICK_LEFT,
        BUTTON_STICK_RIGHT
    };

    float dead_zone = 0.15f;

    Joystick(std::string name, std::vector<uint8_t> buttons, std::vector<float> axis,
             const std::vector<uint8_t> &previous_buttons = {});

    const std::string &name() const;

    const std::vector<uint8_t> &buttons() const;

    const std::vector<float> &axis() const;

    glm::vec2 analog_left() const;

    glm::vec2 analog_right() const;

    glm::vec2 trigger() const;

    glm::vec2 dpad() const;

    const std::unordered_map<Input, Event> &input_events() const;

private:
    std::string m_name;
    std::vector<uint8_t> m_buttons;
    std::vector<float> m_axis;
    std::unordered_map<Input, Event> m_input_events;
};

std::string to_string(Joystick::Input input);

struct Key
{
    enum Type
    {
        _UNKNOWN = -1,
        _SPACEBAR = 32,
        _APOSTROPHE = 39, /* ' */
        _COMMA = 44,      /* , */
        _MINUS = 45,      /* - */
        _PERIOD = 46,     /* . */
        _SLASH = 47,      /* / */
        _0 = 48,
        _1 = 49,
        _2 = 50,
        _3 = 51,
        _4 = 52,
        _5 = 53,
        _6 = 54,
        _7 = 55,
        _8 = 56,
        _9 = 57,
        _SEMICOLON = 59, /* ; */
        _EQUAL = 61,     /* = */
        _A = 65,
        _B = 66,
        _C = 67,
        _D = 68,
        _E = 69,
        _F = 70,
        _G = 71,
        _H = 72,
        _I = 73,
        _J = 74,
        _K = 75,
        _L = 76,
        _M = 77,
        _N = 78,
        _O = 79,
        _P = 80,
        _Q = 81,
        _R = 82,
        _S = 83,
        _T = 84,
        _U = 85,
        _V = 86,
        _W = 87,
        _X = 88,
        _Y = 89,
        _Z = 90,
        _LEFT_BRACKET = 91,  /* [ */
        _BACKSLASH = 92,     /* \ */
        _RIGHT_BRACKET = 93, /* ] */
        _GRAVE_ACCENT = 96,  /* ` */
        _WORLD_1 = 161,      /* non-US #1 */
        _WORLD_2 = 162,      /* non-US #2 */
        _ESCAPE = 256,
        _ENTER = 257,
        _TAB = 258,
        _BACKSPACE = 259,
        _INSERT = 260,
        _DELETE = 261,
        _RIGHT = 262,
        _LEFT = 263,
        _DOWN = 264,
        _UP = 265,
        _PAGE_UP = 266,
        _PAGE_DOWN = 267,
        _HOME = 268,
        _END = 269,
        _CAPS_LOCK = 280,
        _SCROLL_LOCK = 281,
        _NUM_LOCK = 282,
        _PRINT_SCREEN = 283,
        _PAUSE = 284,
        _F1 = 290,
        _F2 = 291,
        _F3 = 292,
        _F4 = 293,
        _F5 = 294,
        _F6 = 295,
        _F7 = 296,
        _F8 = 297,
        _F9 = 298,
        _F10 = 299,
        _F11 = 300,
        _F12 = 301,
        _F13 = 302,
        _F14 = 303,
        _F15 = 304,
        _F16 = 305,
        _F17 = 306,
        _F18 = 307,
        _F19 = 308,
        _F20 = 309,
        _F21 = 310,
        _F22 = 311,
        _F23 = 312,
        _F24 = 313,
        _F25 = 314,
        _KP_0 = 320,
        _KP_1 = 321,
        _KP_2 = 322,
        _KP_3 = 323,
        _KP_4 = 324,
        _KP_5 = 325,
        _KP_6 = 326,
        _KP_7 = 327,
        _KP_8 = 328,
        _KP_9 = 329,
        _KP_DECIMAL = 330,
        _KP_DIVIDE = 331,
        _KP_MULTIPLY = 332,
        _KP_SUBTRACT = 333,
        _KP_ADD = 334,
        _KP_ENTER = 335,
        _KP_EQUAL = 336,
        _LEFT_SHIFT = 340,
        _LEFT_CONTROL = 341,
        _LEFT_ALT = 342,
        _LEFT_SUPER = 343,
        _RIGHT_SHIFT = 344,
        _RIGHT_CONTROL = 345,
        _RIGHT_ALT = 346,
        _RIGHT_SUPER = 347,
        _MENU = 348,
        _LAST = _MENU
    };
};
}// namespace vierkant
