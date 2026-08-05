#ifdef _WIN32

#include <algorithm>
#include <array>
#include <chrono>
#include <string>

#include <windows.h>
// clang-format off
#include <xinput.h>
// clang-format on

// only for the canonical index-constants (GLFW_GAMEPAD_BUTTON_* / GLFW_GAMEPAD_AXIS_*),
// shared with the interpretation-layer in Input.cpp. no glfw joystick-api is used here.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gamepad.hpp"

namespace vierkant::gamepad
{

namespace
{

//! XINPUT_GAMEPAD_* flags in canonical button-order. XInput has no guide-button, that slot stays unset.
constexpr std::array<WORD, GLFW_GAMEPAD_BUTTON_LAST + 1> g_button_flags = {
        XINPUT_GAMEPAD_A,
        XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_X,
        XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER,
        XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_BACK,
        XINPUT_GAMEPAD_START,
        0,
        XINPUT_GAMEPAD_LEFT_THUMB,
        XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_DPAD_UP,
        XINPUT_GAMEPAD_DPAD_RIGHT,
        XINPUT_GAMEPAD_DPAD_DOWN,
        XINPUT_GAMEPAD_DPAD_LEFT};

using rumble_clock_t = std::chrono::steady_clock;

//! XInput vibration runs until it is stopped, so the requested duration is tracked here.
std::array<rumble_clock_t::time_point, XUSER_MAX_COUNT> g_rumble_end = {};

//! XInput sticks are int16 with a full-scale negative side, triggers are uint8
float normalize_stick(SHORT value) { return value < 0 ? value / 32768.f : value / 32767.f; }

//! canonical triggers live in [-1, 1]
float normalize_trigger(BYTE value) { return value / 255.f * 2.f - 1.f; }

}// namespace

std::vector<state_t> poll_states()
{
    std::vector<state_t> ret;
    const auto now = rumble_clock_t::now();

    for(DWORD user_index = 0; user_index < XUSER_MAX_COUNT; ++user_index)
    {
        XINPUT_STATE xinput_state = {};
        if(XInputGetState(user_index, &xinput_state) != ERROR_SUCCESS) { continue; }

        if(g_rumble_end[user_index] != rumble_clock_t::time_point() && now >= g_rumble_end[user_index])
        {
            XINPUT_VIBRATION vibration = {};
            XInputSetState(user_index, &vibration);
            g_rumble_end[user_index] = {};
        }
        const XINPUT_GAMEPAD &pad = xinput_state.Gamepad;

        state_t state;
        state.id = user_index + 1;
        state.name = "xinput gamepad " + std::to_string(user_index);

        state.buttons.assign(g_button_flags.size(), 0);
        for(uint32_t i = 0; i < g_button_flags.size(); ++i)
        {
            state.buttons[i] = (pad.wButtons & g_button_flags[i]) ? 1 : 0;
        }
        state.axis.resize(GLFW_GAMEPAD_AXIS_LAST + 1);
        state.axis[GLFW_GAMEPAD_AXIS_LEFT_X] = normalize_stick(pad.sThumbLX);

        // XInput reports sticks with +y up, the canonical layout has +y down
        state.axis[GLFW_GAMEPAD_AXIS_LEFT_Y] = -normalize_stick(pad.sThumbLY);
        state.axis[GLFW_GAMEPAD_AXIS_RIGHT_X] = normalize_stick(pad.sThumbRX);
        state.axis[GLFW_GAMEPAD_AXIS_RIGHT_Y] = -normalize_stick(pad.sThumbRY);
        state.axis[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = normalize_trigger(pad.bLeftTrigger);
        state.axis[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = normalize_trigger(pad.bRightTrigger);
        ret.push_back(std::move(state));
    }
    return ret;
}

bool rumble(uint64_t device_id, float strong, float weak, uint32_t duration_ms)
{
    if(device_id == 0 || device_id > XUSER_MAX_COUNT) { return false; }
    const auto user_index = static_cast<DWORD>(device_id - 1);

    constexpr float max_magnitude = 0xffff;
    XINPUT_VIBRATION vibration = {};
    vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(strong, 0.f, 1.f) * max_magnitude);
    vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(weak, 0.f, 1.f) * max_magnitude);

    if(XInputSetState(user_index, &vibration) != ERROR_SUCCESS) { return false; }
    g_rumble_end[user_index] = rumble_clock_t::now() + std::chrono::milliseconds(duration_ms);
    return true;
}

}// namespace vierkant::gamepad

#endif// _WIN32
