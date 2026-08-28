#include "vierkant/punctual_light.hpp"
#include <gtest/gtest.h>
#include <random>

using namespace vierkant;
//____________________________________________________________________________//

namespace
{

//! reference-point the weights are evaluated at
constexpr glm::vec3 ref_pos = {0.f, 0.f, 0.f};

//! unit distance from ref_pos, so the 1/d^2 factor drops out of the expected values
light_t make_light(LightType type, float intensity = 1.f, const glm::vec3 &color = glm::vec3(1))
{
    light_t ret = {};
    ret.type = static_cast<uint32_t>(type);
    ret.color = color;
    ret.intensity = intensity;
    ret.position = {0.f, 0.f, 1.f};
    ret.size_x = 1.f;
    ret.size_y = 1.f;
    return ret;
}

float power(const light_t &light) { return light_power(light, ref_pos); }

}// namespace

//____________________________________________________________________________//

TEST(PunctualLight, light_power_delta_convention)
{
    // white has NTSC-luma 1, so a unit-solid-angle light's power is just its intensity
    EXPECT_FLOAT_EQ(power(make_light(LightType::Omni)), 1.f);
    EXPECT_FLOAT_EQ(power(make_light(LightType::Spot)), 1.f);
    EXPECT_FLOAT_EQ(power(make_light(LightType::Directional)), 1.f);

    // extent-less lights ignore size, so omni and spot stay comparable
    auto omni = make_light(LightType::Omni);
    omni.size_x = omni.size_y = 17.f;
    EXPECT_FLOAT_EQ(power(omni), 1.f);
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_scales_with_intensity_and_luminance)
{
    EXPECT_FLOAT_EQ(power(make_light(LightType::Omni, 7.f)), 7.f);

    // a bright light outranks a dim one of the same type - the ordering the selection relies on
    EXPECT_GT(power(make_light(LightType::Omni, 100.f)), power(make_light(LightType::Omni, 0.1f)));

    // color enters through NTSC luma, green weighs more than blue
    EXPECT_FLOAT_EQ(power(make_light(LightType::Omni, 1.f, {0.f, 1.f, 0.f})), 0.587f);
    EXPECT_GT(power(make_light(LightType::Omni, 1.f, {0.f, 1.f, 0.f})),
              power(make_light(LightType::Omni, 1.f, {0.f, 0.f, 1.f})));
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_sun_disc)
{
    // angular_size 0 is a delta sun: unit solid angle
    auto sun = make_light(LightType::Directional);
    EXPECT_FLOAT_EQ(power(sun), 1.f);

    // a disc-sun uses the cap solid angle, matching the pdf in sample_light()
    sun.angular_size = 0.1f;
    EXPECT_FLOAT_EQ(power(sun), glm::two_pi<float>() * (1.f - std::cos(0.1f)));

    // a wider disc gathers more power
    auto wide = sun;
    wide.angular_size = 0.5f;
    EXPECT_GT(power(wide), power(sun));
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_area_extents)
{
    // projected area at unit distance, with the extents sample_light()'s area-pdfs use
    EXPECT_FLOAT_EQ(power(make_light(LightType::Rect)), 4.f);
    EXPECT_FLOAT_EQ(power(make_light(LightType::Disk)), glm::pi<float>());
    EXPECT_FLOAT_EQ(power(make_light(LightType::Sphere)), glm::pi<float>());
    EXPECT_FLOAT_EQ(power(make_light(LightType::Tube)), 4.f);

    // rect power is linear in each half-extent
    auto rect = make_light(LightType::Rect);
    auto wide_rect = rect;
    wide_rect.size_x = 2.f;
    EXPECT_FLOAT_EQ(power(wide_rect), 2.f * power(rect));

    // disk/sphere power is quadratic in the radius
    auto disk = make_light(LightType::Disk);
    auto big_disk = disk;
    big_disk.size_x = 3.f;
    EXPECT_FLOAT_EQ(power(big_disk), 9.f * power(disk));
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_inverse_square_falloff)
{
    // types carrying the 1/d^2 falloff are evaluated at the reference-point
    auto omni = make_light(LightType::Omni, 8.f);
    omni.position = {0.f, 0.f, 2.f};
    EXPECT_FLOAT_EQ(power(omni), 8.f / 4.f);

    auto sphere = make_light(LightType::Sphere, 8.f);
    sphere.position = {0.f, 0.f, 4.f};
    EXPECT_FLOAT_EQ(power(sphere), 8.f * glm::pi<float>() / 16.f);

    // a directional light has no falloff and must ignore the distance
    auto sun = make_light(LightType::Directional, 8.f);
    sun.position = {0.f, 0.f, 1000.f};
    EXPECT_FLOAT_EQ(power(sun), 8.f);

    // a light sitting on the reference-point stays finite
    auto coincident = make_light(LightType::Omni, 1.f);
    coincident.position = ref_pos;
    EXPECT_TRUE(std::isfinite(power(coincident)));
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_sun_outranks_local_lights)
{
    // regression: without the 1/d^2 a local light outranks the sun by d^2, starving it of
    // nee-samples. values are the editor's default sun and the test-scene's spot
    auto sun = make_light(LightType::Directional, 25000.f, {1.f, 0.6f, 0.4f});
    sun.angular_size = glm::radians(0.524167f);

    auto spot = make_light(LightType::Spot, 100.f);
    spot.position = {0.f, 0.f, 5.f};

    // both deliver a comparable amount at 5m, so neither may dominate the selection
    auto bins = create_light_alias_table({sun, spot}, ref_pos, 0.f);
    EXPECT_NEAR(bins[0].prob, 0.53f, 0.05f);
    EXPECT_NEAR(bins[1].prob, 0.47f, 0.05f);
}

//____________________________________________________________________________//

TEST(PunctualLight, light_power_degenerate)
{
    // zero extent yields zero weight
    auto rect = make_light(LightType::Rect);
    rect.size_x = 0.f;
    EXPECT_FLOAT_EQ(power(rect), 0.f);

    // black or zero-intensity lights likewise
    EXPECT_FLOAT_EQ(power(make_light(LightType::Omni, 0.f)), 0.f);
    EXPECT_FLOAT_EQ(power(make_light(LightType::Rect, 1.f, glm::vec3(0))), 0.f);
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
    EXPECT_TRUE(create_light_alias_table({}, ref_pos).empty());

    // a single light is always picked, with probability 1
    auto bins = create_light_alias_table({make_light(LightType::Omni, 3.f)}, ref_pos, 0.f);
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
                                          make_light(LightType::Omni, 6.f)}, ref_pos, 0.f);
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

    auto bins = create_light_alias_table(lights, ref_pos, 0.f);
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
    auto bins = create_light_alias_table(lights, ref_pos, 0.f);
    EXPECT_FLOAT_EQ(bins[1].prob, 0.f);

    auto histogram = sample_histogram(bins, 100000);
    EXPECT_EQ(histogram[1], 0.0);
    EXPECT_NEAR(histogram[0], 0.5, 5.e-3);
    EXPECT_NEAR(histogram[2], 0.5, 5.e-3);
}

//____________________________________________________________________________//

TEST(PunctualLight, alias_table_uniform_mix)
{
    // a dim light among bright ones: the regression a global weight cannot avoid on its own,
    // since a light dim at the reference-point can still dominate the surfaces next to it
    std::vector<light_t> lights = {make_light(LightType::Omni, 100.f), make_light(LightType::Omni, 100.f),
                                   make_light(LightType::Omni, 100.f), make_light(LightType::Omni, 0.1f)};
    const float uniform_p = 1.f / lights.size();

    // fully weighted: the dim light is starved
    auto weighted = create_light_alias_table(lights, ref_pos, 0.f);
    EXPECT_LT(weighted[3].prob, 0.01f * uniform_p);

    // mixing bounds it at uniform_mix / num_lights
    for(float mix: {0.25f, 0.5f, 0.75f})
    {
        auto bins = create_light_alias_table(lights, ref_pos, mix);
        EXPECT_GE(bins[3].prob, mix * uniform_p);
        EXPECT_LT(bins[3].prob, uniform_p);

        float sum = 0.f;
        for(const auto &bin: bins) { sum += bin.prob; }
        EXPECT_NEAR(sum, 1.f, 1.e-6f);
    }

    // fully mixed is plain uniform picking
    auto uniform = create_light_alias_table(lights, ref_pos, 1.f);
    for(const auto &bin: uniform) { EXPECT_FLOAT_EQ(bin.prob, uniform_p); }

    auto histogram = sample_histogram(uniform, 100000);
    for(double p: histogram) { EXPECT_NEAR(p, uniform_p, 5.e-3); }
}

//____________________________________________________________________________//

TEST(PunctualLight, alias_table_all_zero_weights_is_uniform)
{
    // no light carries weight: the table must stay samplable rather than pick nothing
    std::vector<light_t> lights(4, make_light(LightType::Omni, 0.f));
    auto bins = create_light_alias_table(lights, ref_pos, 0.f);

    for(const auto &bin: bins) { EXPECT_FLOAT_EQ(bin.prob, 0.25f); }

    auto histogram = sample_histogram(bins, 100000);
    for(double p: histogram) { EXPECT_NEAR(p, 0.25, 5.e-3); }
}
