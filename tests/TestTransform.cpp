//  See http://www.boost.org/libs/test for the library home page.

// Boost.Test

// each test module could contain no more then one 'main' file with init function defined
// alternatively you could define init function yourself
#define BOOST_TEST_MAIN

#include "vierkant/transform.hpp"
#include <boost/test/tools/floating_point_comparison.hpp>
#include <boost/test/unit_test.hpp>
#include <unordered_set>

using namespace vierkant;
//____________________________________________________________________________//

template<typename T>
void check_transform(T epsilon)
{
    // default construction yields identity
    transform_t_<T> identity = {};
    BOOST_CHECK(identity.translation == (glm::vec<3, T>(0)));
    BOOST_CHECK(identity.rotation == glm::qua<T>(1, 0, 0, 0));
    BOOST_CHECK(identity.scale == (glm::vec<3, T>(1)));

    auto m = mat4_cast<T>(identity);
    BOOST_CHECK(mat4_cast<T>(identity) == (glm::mat<4, 4, T>(1)));

    // check an identity transform
    auto p1 = glm::vec<3, T>(1, 2, 3);
    auto p2 = identity * p1;
    BOOST_CHECK(p1 == p2);

    transform_t_<T> translate = {};
    translate.translation = {3.f, 2.f, 1.f};

    // check translation
    auto tp = translate * p1;
    BOOST_CHECK(tp == (glm::vec<3, T>(4)));

    BOOST_CHECK(identity == identity);
    BOOST_CHECK(identity * identity == identity);
    BOOST_CHECK(translate != identity);
    BOOST_CHECK(translate * identity == translate);

    // check rotation 90 degree around y-axis
    transform_t_<T> rotate = {};
    rotate.rotation = glm::angleAxis(glm::half_pi<T>(), glm::vec<3, T>(0, 1, 0));
    auto tr = rotate * p1;
    const auto tr_expected = glm::vec<3, T>(p1.z, p1.y, -p1.x);

    BOOST_CHECK(glm::all(glm::epsilonEqual(tr, tr_expected, epsilon)));

    // simple scaling
    transform_t_<T> scale = {};
    T scale_val = 0.5;
    scale.scale = glm::vec<3, T>(scale_val);
    BOOST_CHECK(glm::all(glm::epsilonEqual(scale * p1, scale_val * p1, epsilon)));

    auto combo = scale * translate * rotate;
    auto tc = combo * p1;
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc, glm::vec<3, T>(3, 2, 0), epsilon)));

    // chain transform_t
    auto tc1 = (scale * translate * rotate) * p1;
    auto tc2 = scale * (translate * (rotate * p1));
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // mat4 analog
    tc1 = mat4_cast(scale) * mat4_cast(translate) * mat4_cast(rotate) * glm::vec4(p1, 1.f);
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // some more involved transform chain, all in double-precision
    transform_t_<T> a = {}, b = {}, c = {};
    a.translation = {11, 19, -5};
    a.rotation = glm::angleAxis(glm::radians(123.), glm::normalize(glm::dvec3(4, -7, 6)));
    a.scale = glm::dvec3(0.5);

    b.translation = {0, -2, 25};
    b.rotation = glm::angleAxis(glm::radians(-99.), glm::normalize(glm::dvec3(1, 2, -3)));
    b.scale = glm::dvec3(2.5);

    c.translation = {-8, -8, -8};
    c.rotation = glm::angleAxis(glm::radians(69.), glm::normalize(glm::dvec3(0, -1, 0)));
    c.scale = glm::dvec3(1.);

    // combined transforms vs. combined mat4
    tc1 = a * b * c * p1;
    tc2 = mat4_cast<T>(a) * mat4_cast<T>(b) * mat4_cast<T>(c) * glm::vec<4, T>(p1, 1.);
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // check transform inversion
    BOOST_CHECK(vierkant::epsilon_equal<T>(a * vierkant::inverse(a), identity, epsilon));
    BOOST_CHECK(vierkant::epsilon_equal<T>(b * vierkant::inverse(b), identity, epsilon));
    BOOST_CHECK(vierkant::epsilon_equal<T>(c * vierkant::inverse(c), identity, epsilon));
    BOOST_CHECK(vierkant::epsilon_equal<T>(a * b * c * vierkant::inverse(a * b * c), identity, epsilon));

    // hash transform
    std::hash<vierkant::transform_t_<T>> hasher;
    BOOST_CHECK(hasher(rotate));

    // use transform as key in hashset
    std::unordered_set<vierkant::transform_t_<T>> test_set = {rotate, scale};
    BOOST_CHECK(test_set.contains(rotate));
    BOOST_CHECK(test_set.contains(scale));
    BOOST_CHECK(!test_set.contains(translate));

    // check mix-routine
    {
        T v = 0.5;
        c = vierkant::mix(a, b, v);

        auto t = glm::mix(a.translation, b.translation, v);
        auto r = glm::slerp(a.rotation, b.rotation, v);
        auto s = glm::mix(a.scale, b.scale, v);

        BOOST_CHECK(glm::all(glm::epsilonEqual(c.translation, t, epsilon)));
        BOOST_CHECK(glm::all(glm::epsilonEqual(c.rotation, r, epsilon)));
        BOOST_CHECK(glm::all(glm::epsilonEqual(c.scale, s, epsilon)));
    }
};

BOOST_AUTO_TEST_CASE(test_rigid_transform)
{
    // empiric minimal epsilons until test breaks
    constexpr double double_epsilon = 1.e-14;
    constexpr float float_epsilon = 1.e-5f;

    check_transform<float>(float_epsilon);
    check_transform<double>(double_epsilon);
}
