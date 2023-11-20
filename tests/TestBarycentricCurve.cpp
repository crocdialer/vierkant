#include <gtest/gtest.h>
#include <glm/gtc/random.hpp>
#include <vierkant/barycentric_curve.hpp>

using namespace vierkant;
//____________________________________________________________________________//

TEST(BarycentricCurve, basic)
{
    constexpr uint32_t num_iterations = 100;
    constexpr uint32_t num_levels = 4;

    uint32_t num_micro_triangles = vierkant::num_micro_triangles(num_levels);

    // generate a number of random barycentric
    for(uint32_t i = 0; i < num_iterations; ++i)
    {
        glm::vec2 bary = glm::linearRand(glm::vec2(0), glm::vec2(1));
        uint32_t curve_index = vierkant::bary2index(bary.x, bary.y, num_levels);
        EXPECT_TRUE(curve_index < num_micro_triangles);
    }
}

TEST(BarycentricCurve, num_micro_triangles)
{
    EXPECT_EQ(vierkant::num_micro_triangles(0), 1);
    EXPECT_EQ(vierkant::num_micro_triangles(1), 4);
    EXPECT_EQ(vierkant::num_micro_triangles(2), 16);
    EXPECT_EQ(vierkant::num_micro_triangles(3), 64);
    EXPECT_EQ(vierkant::num_micro_triangles(4), 256);
}