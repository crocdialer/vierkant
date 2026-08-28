#include "vierkant/punctual_light.hpp"
#include <gtest/gtest.h>

using namespace vierkant;
//____________________________________________________________________________//

namespace
{

light_t make_light(LightType type, float intensity = 1.f, const glm::vec3 &color = glm::vec3(1))
{
    light_t ret = {};
    ret.type = static_cast<uint32_t>(type);
    ret.color = color;
    ret.intensity = intensity;
    ret.size_x = 1.f;
    ret.size_y = 1.f;
    return ret;
}

}// namespace

//____________________________________________________________________________//

TEST(PunctualLight, light_power_delta_convention)
{
    // white has NTSC-luma 1, so a unit-solid-angle light's power is just its intensity
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Omni)), 1.f);
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Spot)), 1.f);
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Directional)), 1.f);

    // extent-less lights ignore size, so omni and spot stay comparable
    auto omni = make_light(LightType::Omni);
    omni.size_x = omni.size_y = 17.f;
    EXPECT_FLOAT_EQ(light_power(omni), 1.f);
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_scales_with_intensity_and_luminance)
{
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Omni, 7.f)), 7.f);

    // a bright light outranks a dim one of the same type - the ordering the selection relies on
    EXPECT_GT(light_power(make_light(LightType::Omni, 100.f)), light_power(make_light(LightType::Omni, 0.1f)));

    // color enters through NTSC luma, green weighs more than blue
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Omni, 1.f, {0.f, 1.f, 0.f})), 0.587f);
    EXPECT_GT(light_power(make_light(LightType::Omni, 1.f, {0.f, 1.f, 0.f})),
              light_power(make_light(LightType::Omni, 1.f, {0.f, 0.f, 1.f})));
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_sun_disc)
{
    // angular_size 0 is a delta sun: unit solid angle
    auto sun = make_light(LightType::Directional);
    EXPECT_FLOAT_EQ(light_power(sun), 1.f);

    // a disc-sun uses the cap solid angle, matching the pdf in sample_light()
    sun.angular_size = 0.1f;
    EXPECT_FLOAT_EQ(light_power(sun), glm::two_pi<float>() * (1.f - std::cos(0.1f)));

    // a wider disc gathers more power
    auto wide = sun;
    wide.angular_size = 0.5f;
    EXPECT_GT(light_power(wide), light_power(sun));
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_area_extents)
{
    // pi * area, with the extents sample_light()'s area-pdfs use
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Rect)), glm::pi<float>() * 4.f);
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Disk)), glm::pi<float>() * glm::pi<float>());
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Sphere)), glm::pi<float>() * 4.f * glm::pi<float>());
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Tube)), glm::pi<float>() * 4.f * glm::pi<float>());

    // rect power is linear in each half-extent
    auto rect = make_light(LightType::Rect);
    auto wide_rect = rect;
    wide_rect.size_x = 2.f;
    EXPECT_FLOAT_EQ(light_power(wide_rect), 2.f * light_power(rect));

    // disk/sphere power is quadratic in the radius
    auto disk = make_light(LightType::Disk);
    auto big_disk = disk;
    big_disk.size_x = 3.f;
    EXPECT_FLOAT_EQ(light_power(big_disk), 9.f * light_power(disk));
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_degenerate)
{
    // zero extent yields zero weight - such a light must be excluded from the selection-table,
    // rather than selected with pdf 0
    auto rect = make_light(LightType::Rect);
    rect.size_x = 0.f;
    EXPECT_FLOAT_EQ(light_power(rect), 0.f);

    // black or zero-intensity lights likewise
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Omni, 0.f)), 0.f);
    EXPECT_FLOAT_EQ(light_power(make_light(LightType::Rect, 1.f, glm::vec3(0))), 0.f);
}
