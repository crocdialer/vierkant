//  See http://www.boost.org/libs/test for the library home page.

// Boost.Test

// each test module could contain no more then one 'main' file with init function defined
// alternatively you could define init function yourself
#define BOOST_TEST_MAIN

#include "vierkant/transform.hpp"
#include <boost/test/tools/floating_point_comparison.hpp>
#include <boost/test/unit_test.hpp>

using namespace vierkant;
//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE(test_rigid_transform)
{
    // default construction yields identity
    transform_t identity = {};
    BOOST_CHECK(identity.translation == glm::dvec3(0.));
    BOOST_CHECK(identity.rotation == glm::dquat(1., 0., 0., 0.));
    BOOST_CHECK(identity.scale == glm::dvec3(1.));

    auto m = mat4_cast(identity);
    BOOST_CHECK(mat4_cast(identity) == glm::mat4(1.));

    // check an identity transform
    auto p1 = glm::vec3(1.f, 2.f, 3.f);
    auto p2 = identity * p1;
    BOOST_CHECK(p1 == p2);

    transform_t translate = {};
    translate.translation = {3.f, 2.f, 1.f};

    // check translation
    auto tp = translate * p1;
    BOOST_CHECK(tp == glm::vec3(4.f));

    BOOST_CHECK(identity == identity);
    BOOST_CHECK(identity * identity == identity);
    BOOST_CHECK(translate != identity);
    BOOST_CHECK(translate * identity == translate);

    // check rotation 90 degree around y-axis
    transform_t rotate = {};
    rotate.rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.f, 1.f, 0.f));
    auto tr = rotate * p1;
    const auto tr_expected = glm::vec3(p1.z, p1.y, -p1.x);

    constexpr float epsilon = 1.e-6f;
    BOOST_CHECK(glm::all(glm::epsilonEqual(tr, tr_expected, epsilon)));

    // simple scaling
    transform_t scale = {};
    scale.scale = glm::vec3(0.5f);
    BOOST_CHECK(glm::all(glm::epsilonEqual(scale * p1, 0.5f * p1, epsilon)));

    auto combo = scale * translate * rotate;
    auto tc = combo * p1;
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc, glm::vec3(3.f, 2.f, 0.f), epsilon)));

    // chain transform_t
    auto tc1 = (scale * translate * rotate) * p1;
    auto tc2 = scale * (translate * (rotate * p1));
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // mat4 analog
    tc1 = mat4_cast(scale) * mat4_cast(translate) * mat4_cast(rotate) * glm::vec4(p1, 1.f);
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // some more involved transform chain, all in double-precision
    transform_t_<double> a = {}, b = {}, c = {};
    a.translation = {11, 19, -5};
    a.rotation = glm::angleAxis(glm::radians(123.), glm::normalize(glm::dvec3(4, -7, 6)));
    a.scale = glm::dvec3(0.5);

    b.translation = {0, -2, 25};
    b.rotation = glm::angleAxis(glm::radians(-99.), glm::normalize(glm::dvec3(1, 2, -3)));
    b.scale = glm::dvec3(2.5);

    c.translation = {-8, -8, -8};
    c.rotation = glm::angleAxis(glm::radians(69.), glm::normalize(glm::dvec3(0, -1, 0)));
    c.scale = glm::dvec3(1.);

    // combined transforms vs. combined double-precision mat4
    tc1 = a * b * c * p1;
    tc2 = mat4_cast<double>(a) * mat4_cast<double>(b) * mat4_cast<double>(c) * glm::dvec4(p1, 1.);
    BOOST_CHECK(glm::all(glm::epsilonEqual(tc1, tc2, epsilon)));

    // check transform inversion
    BOOST_CHECK(vierkant::epsilon_equal<double>(a * vierkant::inverse(a), identity, epsilon));
    BOOST_CHECK(vierkant::epsilon_equal<double>(b * vierkant::inverse(b), identity, epsilon));
    BOOST_CHECK(vierkant::epsilon_equal<double>(c * vierkant::inverse(c), identity, epsilon));
    BOOST_CHECK(vierkant::epsilon_equal<double>(a * b * c * vierkant::inverse(a * b * c), identity, epsilon));
}
