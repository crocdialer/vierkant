#include "vierkant/punctual_light.hpp"
#include <gtest/gtest.h>
#include <random>

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

//____________________________________________________________________________//

namespace
{

//! histogram of table-draws, normalized to a distribution
std::vector<double> sample_histogram(const std::vector<light_alias_bin_t> &bins, size_t num_draws)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.f, 1.f);
    std::vector<double> histogram(bins.size(), 0.0);

    for(size_t i = 0; i < num_draws; ++i)
    {
        auto index = sample_light_alias_table(bins, dist(rng), dist(rng));
        EXPECT_LT(index, bins.size());
        histogram[index] += 1.0 / num_draws;
    }
    return histogram;
}

}// namespace

//____________________________________________________________________________//

TEST(PunctualLight, alias_table_degenerate_sizes)
{
    EXPECT_TRUE(create_light_alias_table({}).empty());

    // a single light is always picked, with probability 1
    auto bins = create_light_alias_table({make_light(LightType::Omni, 3.f)});
    ASSERT_EQ(bins.size(), 1);
    EXPECT_EQ(bins[0].alias, 0);
    EXPECT_FLOAT_EQ(bins[0].prob, 1.f);
    EXPECT_EQ(sample_light_alias_table(bins, 0.99f, 0.99f), 0);
}

//____________________________________________________________________________//

TEST(PunctualLight, alias_table_probabilities_match_power)
{
    // white omnis, so power == intensity: expected probabilities are 1/10, 3/10, 6/10
    auto bins = create_light_alias_table({make_light(LightType::Omni, 1.f), make_light(LightType::Omni, 3.f),
                                          make_light(LightType::Omni, 6.f)});
    ASSERT_EQ(bins.size(), 3);
    EXPECT_FLOAT_EQ(bins[0].prob, 0.1f);
    EXPECT_FLOAT_EQ(bins[1].prob, 0.3f);
    EXPECT_FLOAT_EQ(bins[2].prob, 0.6f);

    // probabilities are normalized
    float sum = 0.f;
    for(const auto &bin: bins) { sum += bin.prob; }
    EXPECT_NEAR(sum, 1.f, 1.e-6f);
}

//____________________________________________________________________________//

TEST(PunctualLight, alias_table_sampling_reproduces_weights)
{
    // one bright light among many dim ones - the case the whole table exists for
    std::vector<light_t> lights = {make_light(LightType::Omni, 100.f)};
    for(uint32_t i = 0; i < 15; ++i) { lights.push_back(make_light(LightType::Omni, 1.f)); }

    auto bins = create_light_alias_table(lights);
    auto histogram = sample_histogram(bins, 400000);

    for(size_t i = 0; i < bins.size(); ++i) { EXPECT_NEAR(histogram[i], bins[i].prob, 5.e-3); }

    // the bright light takes 100/115 of the draws, instead of the uniform 1/16
    EXPECT_NEAR(histogram[0], 100.0 / 115.0, 5.e-3);
}

//____________________________________________________________________________//

TEST(PunctualLight, alias_table_zero_weight_lights)
{
    // a zero-weight light must never be drawn, and carries pdf 0 so hit-side MIS falls back
    // to bsdf-sampling for it
    std::vector<light_t> lights = {make_light(LightType::Omni, 2.f), make_light(LightType::Omni, 0.f),
                                   make_light(LightType::Omni, 2.f)};
    auto bins = create_light_alias_table(lights);
    EXPECT_FLOAT_EQ(bins[1].prob, 0.f);

    auto histogram = sample_histogram(bins, 100000);
    EXPECT_EQ(histogram[1], 0.0);
    EXPECT_NEAR(histogram[0], 0.5, 5.e-3);
    EXPECT_NEAR(histogram[2], 0.5, 5.e-3);
}

//____________________________________________________________________________//

TEST(PunctualLight, alias_table_all_zero_weights_is_uniform)
{
    // no light carries weight: the table must stay samplable rather than pick nothing
    std::vector<light_t> lights(4, make_light(LightType::Omni, 0.f));
    auto bins = create_light_alias_table(lights);

    for(const auto &bin: bins) { EXPECT_FLOAT_EQ(bin.prob, 0.25f); }

    auto histogram = sample_histogram(bins, 100000);
    for(double p: histogram) { EXPECT_NEAR(p, 0.25, 5.e-3); }
}
