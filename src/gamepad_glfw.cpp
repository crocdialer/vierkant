#if !defined(__linux__) && !defined(_WIN32)

#include <unordered_set>

#include <crocore/crocore.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gamepad.hpp"

namespace vierkant::gamepad
{

/**
 * @brief   fallback-backend for platforms without a native one.
 *
 * macos cannot be done the way linux/windows are: IOHIDManager reports face-buttons as
 * "button 1..N" with no semantics attached, so a mapping-database is unavoidable there.
 * that database is exactly what glfw provides, so glfw's gamepad-api stays in use.
 * force-feedback is unavailable, glfw has no api for it.
 */

namespace
{

//! warn once per device that we have no mapping for it and are ignoring it
void warn_unmapped_joystick(int joystick)
{
    static std::unordered_set<std::string> s_warned_guids;
    const char *guid = glfwGetJoystickGUID(joystick);
    if(!guid || !s_warned_guids.insert(guid).second) { return; }
    const char *name = glfwGetJoystickName(joystick);

    spdlog::warn("no gamepad-mapping for '{}' (guid: {}) - device is ignored", name ? name : "unknown", guid);
}

}// namespace

std::vector<state_t> poll_states()
{
    std::vector<state_t> ret;

    for(int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; ++i)
    {
        if(!glfwJoystickPresent(i)) { continue; }

        // glfw resolves any mapped device into a fixed layout, using its bundled SDL-mapping database.
        // devices it has no mapping for are dropped: raw indices are driver-dependent
        GLFWgamepadstate gamepad_state;
        if(!glfwJoystickIsGamepad(i) || !glfwGetGamepadState(i, &gamepad_state))
        {
            warn_unmapped_joystick(i);
            continue;
        }
        state_t state;
        state.id = static_cast<uint64_t>(i) + 1;
        state.name = glfwGetGamepadName(i);
        state.axis = {std::begin(gamepad_state.axes), std::end(gamepad_state.axes)};
        state.buttons = {std::begin(gamepad_state.buttons), std::end(gamepad_state.buttons)};
        ret.push_back(std::move(state));
    }
    return ret;
}

bool rumble(uint64_t /*device_id*/, float /*strong*/, float /*weak*/, uint32_t /*duration_ms*/) { return false; }

}// namespace vierkant::gamepad

#endif// !__linux__ && !_WIN32
