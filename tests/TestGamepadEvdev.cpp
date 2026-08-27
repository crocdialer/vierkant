#ifdef __linux__

#include <gtest/gtest.h>

#include "gamepad_evdev.hpp"

using namespace vierkant::gamepad;

namespace
{

//! build a synthetic EV_ABS capability-bitmap, as returned by EVIOCGBIT(EV_ABS, ...)
struct abs_caps_t
{
    unsigned long bits[num_longs(ABS_MAX)] = {};

    explicit abs_caps_t(std::initializer_list<int> codes)
    {
        for(int code: codes) { bits[code / bits_per_long] |= 1ul << (code % bits_per_long); }
    }
};

input_absinfo make_absinfo(int32_t value, int32_t minimum, int32_t maximum)
{
    input_absinfo abs = {};
    abs.value = value;
    abs.minimum = minimum;
    abs.maximum = maximum;
    return abs;
}

}// namespace

TEST(GamepadEvdev, axis_layout_is_detected_from_capabilities)
{
    // hid-generic: xbox-pads over bluetooth. right-stick on Z/RZ, triggers on BRAKE/GAS
    abs_caps_t hid_generic({ABS_X, ABS_Y, ABS_Z, ABS_RZ, ABS_GAS, ABS_BRAKE, ABS_HAT0X, ABS_HAT0Y});
    auto layout = detect_axis_layout(hid_generic.bits);
    EXPECT_EQ(layout.right_x, ABS_Z);
    EXPECT_EQ(layout.right_y, ABS_RZ);
    EXPECT_EQ(layout.trigger_left, ABS_BRAKE);
    EXPECT_EQ(layout.trigger_right, ABS_GAS);

    // xpad / xpadneo / steam's virtual pad: right-stick on RX/RY, triggers on Z/RZ
    abs_caps_t xpad({ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y});
    layout = detect_axis_layout(xpad.bits);
    EXPECT_EQ(layout.right_x, ABS_RX);
    EXPECT_EQ(layout.right_y, ABS_RY);
    EXPECT_EQ(layout.trigger_left, ABS_Z);
    EXPECT_EQ(layout.trigger_right, ABS_RZ);
}

TEST(GamepadEvdev, only_both_pedal_axes_select_the_hid_generic_layout)
{
    // GAS alone is not enough: a pad advertising just one pedal-axis is not the hid-generic layout
    abs_caps_t gas_only({ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_GAS});
    EXPECT_EQ(detect_axis_layout(gas_only.bits).right_x, ABS_RX);
}

TEST(GamepadEvdev, axes_are_normalized_from_the_reported_range)
{
    // ranges differ wildly per driver, they must never be hardcoded.
    // hid-generic sticks: 0..65535, xpad sticks: -32768..32767 - both map onto the same [-1, 1]
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(0, 0, 65535), true), -1.f);
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(65535, 0, 65535), true), 1.f);
    EXPECT_NEAR(normalize_axis(make_absinfo(32768, 0, 65535), true), 0.f, 1e-4f);

    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(-32768, -32768, 32767), true), -1.f);
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(32767, -32768, 32767), true), 1.f);

    // triggers: 0..1023 here, 0..255 under xpad
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(0, 0, 1023), false), 0.f);
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(1023, 0, 1023), false), 1.f);
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(255, 0, 255), false), 1.f);
}

TEST(GamepadEvdev, out_of_range_and_degenerate_axes_stay_bounded)
{
    // a value outside the advertised range must not escape [-1, 1]
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(70000, 0, 65535), true), 1.f);
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(-5, 0, 65535), true), -1.f);

    // an axis the device does not actually have reads back as a zeroed absinfo
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(0, 0, 0), true), 0.f);
    EXPECT_FLOAT_EQ(normalize_axis(make_absinfo(0, 0, 0), false), 0.f);
}

//! regression: a pad that has not reported yet used to read as both sticks fully deflected,
//! because the kernel's zero-initialized axis-value is the minimum of an unsigned stick-range.
TEST(GamepadEvdev, an_unreported_stick_is_recognized)
{
    // hid-generic sticks are 0..65535 and rest near the middle, so 0 cannot be a rest-position
    EXPECT_TRUE(axis_unreported(make_absinfo(0, 0, 65535)));
    EXPECT_FALSE(axis_unreported(make_absinfo(32768, 0, 65535)));
    EXPECT_FALSE(axis_unreported(make_absinfo(65535, 0, 65535)));

    // xpad sticks are -32768..32767 and rest at 0, which is why the defect never showed there
    EXPECT_FALSE(axis_unreported(make_absinfo(0, -32768, 32767)));
    EXPECT_FALSE(axis_unreported(make_absinfo(-32768, -32768, 32767)));
}

#endif// __linux__
