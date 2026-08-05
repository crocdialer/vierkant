#pragma once

#ifdef __linux__

#include <cstddef>
#include <linux/input.h>

namespace vierkant::gamepad
{

//! bit-array helpers, matching the layout returned by EVIOCGBIT/EVIOCGKEY
constexpr size_t bits_per_long = 8 * sizeof(unsigned long);

constexpr size_t num_longs(size_t num_bits) { return (num_bits - 1) / bits_per_long + 1; }

constexpr bool test_bit(size_t bit, const unsigned long *bits)
{
    return (bits[bit / bits_per_long] >> (bit % bits_per_long)) & 1ul;
}

//! evdev-codes a pad puts its right-stick and triggers on. two layouts exist in the wild.
struct axis_layout_t
{
    int right_x, right_y, trigger_left, trigger_right;
};

/**
 * @brief   determine the axis-layout from a device's EV_ABS capability-bitmap.
 *
 * hid-generic (xbox-pads over bluetooth) puts the right-stick on ABS_Z/ABS_RZ and the triggers on
 * ABS_BRAKE/ABS_GAS, xpad/xpadneo/steam's virtual pad use ABS_RX/ABS_RY and ABS_Z/ABS_RZ.
 * that is the entire ambiguity, and it is resolved by capability rather than by device-identity.
 */
axis_layout_t detect_axis_layout(const unsigned long *abs_bits);

//! map a raw axis-value into [0, 1], or into [-1, 1] if @p centered.
float normalize_axis(const input_absinfo &abs, bool centered);

}// namespace vierkant::gamepad

#endif// __linux__
