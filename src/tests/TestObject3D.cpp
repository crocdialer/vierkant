//  See http://www.boost.org/libs/test for the library home page.

// Boost.Test

// each test module could contain no more then one 'main' file with init function defined
// alternatively you could define init function yourself
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>
#include "vierkant/Object3D.hpp"

using namespace vierkant;
//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_Object3D )
{
    Object3DPtr a(Object3D::create()), b(Object3D::create()), c(Object3D::create());

    a->set_parent(b);
    BOOST_CHECK(a->parent() == b);
    BOOST_CHECK(b->children().size() == 1);

    b->remove_child(a);
    BOOST_CHECK(!a->parent());

    b->remove_child(a);

    a->add_child(b);
    BOOST_CHECK(a->children().size() == 1);
    BOOST_CHECK(b->parent() == a);

    b->set_parent(Object3DPtr());
    BOOST_CHECK(a->children().empty());
    BOOST_CHECK(!b->parent());

    // a -> b -> c
    c->set_parent(b);
    a->add_child(b);
    BOOST_CHECK(c->parent() == b);
    BOOST_CHECK(b->parent() == a);

    //test scaling
    b->set_scale(0.5);
    c->set_scale(0.2);
    BOOST_CHECK(a->global_scale() == glm::vec3(1));
    BOOST_CHECK(b->global_scale() == glm::vec3(0.5));
    BOOST_CHECK(c->global_scale() == glm::vec3(0.1));

    a->set_position(glm::vec3(0, 100, 0));
    b->set_position(glm::vec3(0, 50, 0));
    BOOST_CHECK(b->position() == glm::vec3(0, 50, 0));
    BOOST_CHECK(b->global_position() == glm::vec3(0, 150, 0));

    // rotations
    b->set_rotation(48.f, 10.f, 5.f);
//    auto poop_scale = b->global_scale();
//    BOOST_CHECK(poop_scale == glm::vec3(0.5));
    BOOST_CHECK(b->global_position() == glm::vec3(0, 150, 0));

    // test set_global_*
    b->set_global_position(glm::vec3(69.f));
    BOOST_CHECK(b->global_position() == glm::vec3(69.f));

    b->set_global_scale(glm::vec3(17.f));
    BOOST_CHECK(b->global_scale() == glm::vec3(17.f));
}

//____________________________________________________________________________//

// EOF
