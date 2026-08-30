#include "vierkant/nodes.hpp"
#include <gtest/gtest.h>

using namespace vierkant;

//! a clip whose first key is not at zero, like a Blender/Mixamo export starting at frame 1
constexpr float test_start_time = 1.f / 30.f;
constexpr float test_duration = 1.f;

TEST(Animation, update_animation_wraps_into_clip_range)
{
    nodes::node_animation_t animation;
    animation.start_time = test_start_time;
    animation.duration = test_duration;

    animation_component_t animation_cmp;
    animation_cmp.current_time = animation.start_time;

    // two full cycles at 60Hz
    for(uint32_t i = 0; i < 120; ++i)
    {
        update_animation(animation, 1.0 / 60.0, animation_cmp);
        EXPECT_GE(animation_cmp.current_time, animation.start_time);
        EXPECT_LE(animation_cmp.current_time, animation.start_time + animation.duration);
    }
}

TEST(Animation, clip_tail_is_not_wrapped_before_the_first_key)
{
    animation_value_t<glm::vec3> first = {}, last = {};
    first.value = glm::vec3(0.f);
    last.value = glm::vec3(10.f, 0.f, 0.f);

    animation_keys_t keys;
    keys.positions[test_start_time] = first;
    keys.positions[test_start_time + test_duration] = last;

    nodes::node_animation_t animation;
    animation.start_time = test_start_time;
    animation.duration = test_duration;

    animation_component_t animation_cmp;
    animation_cmp.current_time = animation.start_time + animation.duration - 2.f / 60.f;

    // the tail of the clip is past duration, but not past the last key: no wrap here
    update_animation(animation, 1.0 / 60.0, animation_cmp);
    EXPECT_GT(animation_cmp.current_time, animation.start_time);

    transform_t transform;
    ASSERT_TRUE(create_animation_transform(keys, static_cast<float>(animation_cmp.current_time),
                                           InterpolationMode::Linear, transform));
    EXPECT_GT(transform.translation.x, 0.f);
}
