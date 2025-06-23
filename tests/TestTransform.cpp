#include "vierkant/transform.hpp"
#include <gtest/gtest.h>
#include <unordered_set>

using namespace vierkant;
//____________________________________________________________________________//

template<typename T>
void check_transform(T epsilon)
{
    // default construction yields identity
    transform_t_<T> identity = {};
    EXPECT_TRUE(identity.translation == (glm::vec<3, T>(0)));
    EXPECT_TRUE(identity.rotation == glm::qua<T>(1, 0, 0, 0));
    EXPECT_TRUE(identity.scale == (glm::vec<3, T>(1)));

    auto m = mat4_cast<T>(identity);
    EXPECT_TRUE(mat4_cast<T>(identity) == (glm::mat<4, 4, T>(1)));

    // check an identity transform
    auto p1 = glm::vec<3, T>(1, 2, 3);
    auto p2 = identity * p1;
    EXPECT_TRUE(p1 == p2);

    transform_t_<T> translate = {};
    translate.translation = {3.f, 2.f, 1.f};

    // check translation
    auto tp = translate * p1;
    EXPECT_TRUE(tp == (glm::vec<3, T>(4)));

    EXPECT_TRUE(identity == identity);
    EXPECT_TRUE(identity * identity == identity);
    EXPECT_TRUE(translate != identity);
    EXPECT_TRUE(translate * identity == translate);

    // check rotation 90 degree around y-axis
    transform_t_<T> rotate = {};
    rotate.rotation = glm::angleAxis(glm::half_pi<T>(), glm::vec<3, T>(0, 1, 0));
    auto tr = rotate * p1;
    const auto tr_expected = glm::vec<3, T>(p1.z, p1.y, -p1.x);

    EXPECT_TRUE(glm::all(glm::epsilonEqual(tr, tr_expected, epsilon)));

    // simple scaling
    transform_t_<T> scale = {};
    glm::vec<3, T> scale_val = {0.5, 1.0, 1.7};
    scale.scale = scale_val;
    EXPECT_TRUE(glm::all(glm::epsilonEqual(scale * p1, scale_val * p1, epsilon)));

    vierkant::transform_t_<T> combo = scale * translate * rotate;
    auto combo_mat = mat4_cast<T>(scale) * mat4_cast<T>(translate) * mat4_cast<T>(rotate);

    glm::vec<3, T> tc1 = combo * p1;
    glm::vec<3, T> tc2 = combo_mat * glm::vec<4, T>(p1, 1.);
    EXPECT_TRUE(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // chain transform_t
    tc1 = (scale * translate * rotate) * p1;
    tc2 = scale * (translate * (rotate * p1));
    EXPECT_TRUE(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // mat4 analog
    tc1 = mat4_cast<T>(scale) * mat4_cast<T>(translate) * mat4_cast<T>(rotate) * glm::vec<4, T>(p1, 1.);
    EXPECT_TRUE(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // some more involved transform chain, all in double-precision
    transform_t_<T> a = {}, b = {}, c = {};
    a.translation = {11, 19, -5};
    a.rotation = glm::angleAxis(glm::radians(123.), glm::normalize(glm::dvec3(4, -7, 6)));
    a.scale = glm::dvec3(0.5, 0.5, 0.5);

    b.translation = {0, -2, 25};
    b.rotation = glm::angleAxis(glm::radians(-99.), glm::normalize(glm::dvec3(1, 2, -3)));
    b.scale = glm::dvec3(2.5);

    c.translation = {-8, -8, -8};
    c.rotation = glm::angleAxis(glm::radians(69.), glm::normalize(glm::dvec3(0, -1, 0)));
    c.scale = glm::dvec3(1.);

    // combined transforms vs. combined mat4
    combo = a * b * c;
    combo_mat = mat4_cast<T>(a) * mat4_cast<T>(b) * mat4_cast<T>(c);

    tc1 = combo * p1;
    tc2 = combo_mat * glm::vec<4, T>(p1, 1.);
    EXPECT_TRUE(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // check transform inversion
    EXPECT_TRUE(vierkant::epsilon_equal<T>(a * vierkant::inverse(a), identity, epsilon));
    EXPECT_TRUE(vierkant::epsilon_equal<T>(b * vierkant::inverse(b), identity, epsilon));
    EXPECT_TRUE(vierkant::epsilon_equal<T>(c * vierkant::inverse(c), identity, epsilon));
    EXPECT_TRUE(vierkant::epsilon_equal<T>(a * b * c * vierkant::inverse(a * b * c), identity, epsilon));

    // hash transform
    std::hash<vierkant::transform_t_<T>> hasher;
    EXPECT_TRUE(hasher(rotate));

    // use transform as key in hashset
    std::unordered_set<vierkant::transform_t_<T>> test_set = {rotate, scale};
    EXPECT_TRUE(test_set.contains(rotate));
    EXPECT_TRUE(test_set.contains(scale));
    EXPECT_TRUE(!test_set.contains(translate));

    // check mix-routine
    {
        T v = 0.5;
        c = vierkant::mix(a, b, v);

        auto t = glm::mix(a.translation, b.translation, v);
        auto r = glm::slerp(a.rotation, b.rotation, v);
        auto s = glm::mix(a.scale, b.scale, v);

        EXPECT_TRUE(glm::all(glm::epsilonEqual(c.translation, t, epsilon)));
        EXPECT_TRUE(glm::all(glm::epsilonEqual(c.rotation, r, epsilon)));
        EXPECT_TRUE(glm::all(glm::epsilonEqual(c.scale, s, epsilon)));
    }
};

TEST(Transform, rigid_transform)
{
    // empiric minimal epsilons until test breaks
    constexpr double double_epsilon = 1.e-14;
    constexpr float float_epsilon = 1.e-5f;

    check_transform<float>(float_epsilon);
    check_transform<double>(double_epsilon);
}

//// TODO: scaffolding for a simple performance-benchmark/comparison
//{
//    vierkant::transform_t test_transform;
//    glm::mat4 test_mat(1);
//    glm::vec3 p;
//    size_t num_iterations = 1000000;
//
//    // bs benchmark
//    {
//        spdlog::stopwatch sw;
//        for(uint32_t i = 0; i < num_iterations; ++i){ test_transform * p; }
//        spdlog::info("vierkant::transform * vec3 {}", std::chrono::duration_cast<std::chrono::nanoseconds>(sw.elapsed() / num_iterations));
//    }
//    {
//        spdlog::stopwatch sw;
//        for(uint32_t i = 0; i < num_iterations; ++i){ test_mat * glm::vec4(p, 1.f); }
//        spdlog::info("glm::mat4 * vec4 {}", std::chrono::duration_cast<std::chrono::nanoseconds>(sw.elapsed() / num_iterations));
//    }
//} // measured almost identical timings for vierkant::transform vs. glm::mat4 (e.g. 124ns vs. 126ns)
