#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vierkant::gamepad
{

/**
 * @brief   internal, platform-specific gamepad-backend.
 *
 * reads gamepads directly from the platform (evdev on linux, XInput on windows), rather than through
 * glfw's joystick-api. both platforms report *named* inputs, so there is no mapping-database involved.
 *
 * @note    not thread-safe, expected to be driven from the thread pumping window-events.
 */

//! per-device snapshot, in canonical gamepad-order (GLFW_GAMEPAD_BUTTON_* / GLFW_GAMEPAD_AXIS_*)
struct state_t
{
    //! opaque device-handle, stable while the device stays connected. never reused.
    uint64_t id = 0;

    //! display-name, as reported by the device
    std::string name;

    //! 15 button-states
    std::vector<uint8_t> buttons;

    //! 6 axis-values. sticks in [-1, 1], triggers in [-1, 1] (glfw-convention, rescaled by Joystick)
    std::vector<float> axis;
};

//! enumerate and poll all connected gamepads. handles hotplug internally.
std::vector<state_t> poll_states();

/**
 * @brief   trigger force-feedback on a device.
 *
 * @param   device_id       a state_t::id from the most recent poll_states()
 * @param   strong          low-frequency motor magnitude in [0, 1]
 * @param   weak            high-frequency motor magnitude in [0, 1]
 * @param   duration_ms     playback-duration in milliseconds
 * @return  false if the device is unknown, gone, or has no force-feedback.
 */
bool rumble(uint64_t device_id, float strong, float weak, uint32_t duration_ms);

}// namespace vierkant::gamepad
