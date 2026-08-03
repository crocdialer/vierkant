#include <gtest/gtest.h>
#include <unordered_set>
#include <vierkant/Input.hpp>

using namespace vierkant;

//! canonical (glfw gamepad) order: LEFT_X, LEFT_Y, RIGHT_X, RIGHT_Y, LEFT_TRIGGER, RIGHT_TRIGGER
std::vector<float> canonical_axis(float lx, float ly, float rx, float ry, float lt, float rt)
{
    return {lx, ly, rx, ry, lt, rt};
}

//! canonical (glfw gamepad) order, 15 entries: A B X Y LB RB BACK START GUIDE LS RS DPU DPR DPD DPL
std::vector<uint8_t> canonical_buttons(std::initializer_list<uint32_t> pressed)
{
    std::vector<uint8_t> ret(15, 0);
    for(auto i: pressed) { ret[i] = 1; }
    return ret;
}

TEST(Input, gamepad_axes_are_read_from_canonical_slots)
{
    // sticks fully deflected, left trigger fully pressed, right trigger released
    Joystick js("pad", canonical_buttons({}), canonical_axis(1.f, 0.f, 0.f, -1.f, 1.f, -1.f));

    EXPECT_FLOAT_EQ(js.analog_left().x, 1.f);
    EXPECT_FLOAT_EQ(js.analog_left().y, 0.f);
    EXPECT_FLOAT_EQ(js.analog_right().x, 0.f);
    EXPECT_FLOAT_EQ(js.analog_right().y, -1.f);

    // triggers arrive in [-1, 1] and are rescaled to [0, 1]
    EXPECT_FLOAT_EQ(js.trigger().x, 1.f);
    EXPECT_FLOAT_EQ(js.trigger().y, 0.f);
}

TEST(Input, gamepad_dpad_is_read_from_canonical_slots)
{
    EXPECT_EQ(Joystick("pad", canonical_buttons({11}), canonical_axis(0, 0, 0, 0, -1, -1)).dpad(),
              glm::vec2(0.f, 1.f));// up
    EXPECT_EQ(Joystick("pad", canonical_buttons({12}), canonical_axis(0, 0, 0, 0, -1, -1)).dpad(),
              glm::vec2(1.f, 0.f));// right
    EXPECT_EQ(Joystick("pad", canonical_buttons({13}), canonical_axis(0, 0, 0, 0, -1, -1)).dpad(),
              glm::vec2(0.f, -1.f));// down
    EXPECT_EQ(Joystick("pad", canonical_buttons({14}), canonical_axis(0, 0, 0, 0, -1, -1)).dpad(),
              glm::vec2(-1.f, 0.f));// left
}

TEST(Input, gamepad_button_events_are_edge_triggered)
{
    auto previous = canonical_buttons({});
    Joystick pressed("pad", canonical_buttons({0}), canonical_axis(0, 0, 0, 0, -1, -1), previous);

    ASSERT_EQ(pressed.input_events().size(), 1);
    ASSERT_TRUE(pressed.input_events().contains(Joystick::Input::BUTTON_A));
    EXPECT_EQ(pressed.input_events().at(Joystick::Input::BUTTON_A), Joystick::Event::BUTTON_PRESS);

    // holding the same button is not an event
    Joystick held("pad", canonical_buttons({0}), canonical_axis(0, 0, 0, 0, -1, -1), pressed.buttons());
    EXPECT_TRUE(held.input_events().empty());

    Joystick released("pad", canonical_buttons({}), canonical_axis(0, 0, 0, 0, -1, -1), held.buttons());
    ASSERT_TRUE(released.input_events().contains(Joystick::Input::BUTTON_A));
    EXPECT_EQ(released.input_events().at(Joystick::Input::BUTTON_A), Joystick::Event::BUTTON_RELEASE);
}

//! regression: every canonical button-index maps to a distinct Input, so none can alias onto another.
//! the guide-button used to fall through an unmapped index and emit a bogus ANALOG_LEFT_X event.
TEST(Input, no_button_index_aliases_another_input)
{
    std::unordered_set<Joystick::Input> seen;

    for(uint32_t i = 0; i < 15; ++i)
    {
        Joystick js("pad", canonical_buttons({i}), canonical_axis(0, 0, 0, 0, -1, -1), canonical_buttons({}));
        ASSERT_EQ(js.input_events().size(), 1) << "button " << i << " produced no unique event";

        const auto input = js.input_events().begin()->first;
        EXPECT_NE(input, Joystick::Input::ANALOG_LEFT_X) << "button " << i << " aliased onto an analog-axis";
        EXPECT_TRUE(seen.insert(input).second) << "button " << i << " aliased onto an already-used input";
    }
    EXPECT_EQ(seen.size(), 15);
}

//! devices glfw has no mapping for keep the pre-gamepad-api behaviour: raw indices are translated
//! from the layout vierkant used to hardcode (kernel-xpad axes, hid-generic dpad).
TEST(Input, unmapped_joystick_falls_back_to_the_legacy_layout)
{
    // raw layout: axes [LX, LY, LT, RX, RY, RT], dpad on buttons 15..18
    std::vector<float> raw_axis = {1.f, 0.f, 1.f, 0.f, -1.f, -1.f};
    std::vector<uint8_t> raw_buttons(19, 0);
    raw_buttons[2] = 1; // X
    raw_buttons[16] = 1;// dpad right

    Joystick js("raw pad", raw_buttons, raw_axis, {}, false);

    EXPECT_FLOAT_EQ(js.analog_left().x, 1.f);
    EXPECT_FLOAT_EQ(js.analog_right().y, -1.f);
    EXPECT_FLOAT_EQ(js.trigger().x, 1.f);// raw axis 2 -> left trigger
    EXPECT_FLOAT_EQ(js.trigger().y, 0.f);
    EXPECT_EQ(js.dpad(), glm::vec2(1.f, 0.f));

    // translated into canonical order, so downstream sees the same shape as a mapped device
    ASSERT_EQ(js.buttons().size(), 15);
    ASSERT_EQ(js.axis().size(), 6);
    EXPECT_TRUE(js.buttons()[2]); // X stays at canonical 2
    EXPECT_TRUE(js.buttons()[12]);// raw 16 -> canonical dpad-right
}

//! a device reporting fewer axes/buttons than the canonical layout must not read out of bounds
TEST(Input, degenerate_devices_are_survivable)
{
    Joystick empty("nothing", {}, {});
    EXPECT_EQ(empty.analog_left(), glm::vec2(0.f));
    EXPECT_EQ(empty.analog_right(), glm::vec2(0.f));
    EXPECT_EQ(empty.trigger(), glm::vec2(0.f));
    EXPECT_EQ(empty.dpad(), glm::vec2(0.f));
    EXPECT_TRUE(empty.input_events().empty());

    // two axes, no buttons, via the legacy path
    Joystick partial("stick", {}, {1.f, 1.f}, {}, false);
    EXPECT_FLOAT_EQ(partial.analog_left().x, 1.f);
    EXPECT_EQ(partial.analog_right(), glm::vec2(0.f));
    EXPECT_EQ(partial.trigger(), glm::vec2(0.f));// absent triggers rest at -1 -> 0
    EXPECT_EQ(partial.dpad(), glm::vec2(0.f));
}
