#include "vierkant/intersection.hpp"
#include <gtest/gtest.h>

//____________________________________________________________________________//

TEST(AABB, basic)
{
    vierkant::AABB a(glm::vec3(-.5f), glm::vec3(.5f)), b;

    // valid/invalid check
    EXPECT_TRUE(a.valid());
    EXPECT_FALSE(b);

    // combine with invalid aabb
    a += b;
    EXPECT_TRUE(a);
    EXPECT_EQ(a.width(), a.size().x);
    EXPECT_EQ(a.width(), 1.f);
    EXPECT_EQ(a.height(), a.size().y);
    EXPECT_EQ(a.height(), 1.f);
    EXPECT_EQ(a.depth(), a.size().z);
    EXPECT_EQ(a.depth(), 1.f);
    EXPECT_EQ(a.center(), glm::vec3(0.f));

    auto a_shifted = a.transform(glm::translate(glm::vec3(1.f)));
    EXPECT_EQ(a_shifted.width(), 1.f);
    EXPECT_EQ(a_shifted.height(), 1.f);
    EXPECT_EQ(a_shifted.depth(), 1.f);
    EXPECT_EQ(a_shifted.center(), glm::vec3(1.f));

    auto a_shifted_alt = a.transform(vierkant::transform_t{.translation = glm::vec3(1.f)});
    EXPECT_EQ(a_shifted, a_shifted_alt);

    a += a_shifted;
    EXPECT_EQ(a.width(), 2.f);
    EXPECT_EQ(a.height(), 2.f);
    EXPECT_EQ(a.depth(), 2.f);
    EXPECT_EQ(a.center(), glm::vec3(.5f));

    // invalid aabb stays invalid after transform
    EXPECT_FALSE(b.transform(glm::translate(glm::vec3(1.f))).valid());
}