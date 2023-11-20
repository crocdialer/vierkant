#include <glm/gtc/random.hpp>
#include <gtest/gtest.h>
#include <vierkant/barycentric_indexing.hpp>

using namespace vierkant;
//____________________________________________________________________________//

TEST(BarycentricIndexing, map_back_forth)
{
    constexpr uint32_t max_num_levels = 6;

    for(uint32_t num_levels = 0; num_levels <= max_num_levels; ++num_levels)
    {
        uint32_t num_micro_triangles = vierkant::num_micro_triangles(num_levels);

        // generate barycentrics for all micro-triangle indices
        for(uint32_t i = 0; i < num_micro_triangles; ++i)
        {
            // transform micromap-index to micro-vertex uvs
            glm::vec2 uv0, uv1, uv2;
            vierkant::index2bary(i, num_levels, uv0, uv1, uv2);
            auto micro_triangle_center = (uv0 + uv1 + uv2) / 3.f;

            // now backwards, transform uv -> micromap-index
            auto result_index = bary2index(micro_triangle_center, num_levels);

            // expect indices to match
            EXPECT_EQ(i, result_index);
        }
    }
}

TEST(BarycentricIndexing, num_micro_triangles)
{
    EXPECT_EQ(vierkant::num_micro_triangles(0), 1);
    EXPECT_EQ(vierkant::num_micro_triangles(1), 4);
    EXPECT_EQ(vierkant::num_micro_triangles(2), 16);
    EXPECT_EQ(vierkant::num_micro_triangles(3), 64);
    EXPECT_EQ(vierkant::num_micro_triangles(4), 256);
}