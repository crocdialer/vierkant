//  See http://www.boost.org/libs/test for the library home page.

// Boost.Test

// each test module could contain no more then one 'main' file with init function defined
// alternatively you could define init function yourself
#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>
#include "vierkant/Object3D.hpp"
#include "vierkant/Scene.hpp"

using namespace vierkant;
//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE(test_Object3D_hierarchy)
{
    Object3DPtr a(Object3D::create()), b(Object3D::create()), c(Object3D::create());

    a->set_parent(b);
    BOOST_CHECK(a->parent() == b);
    BOOST_CHECK(b->children.size() == 1);

    b->remove_child(a);
    BOOST_CHECK(!a->parent());

    b->remove_child(a);

    a->add_child(b);
    BOOST_CHECK(a->children.size() == 1);
    BOOST_CHECK(b->parent() == a);

    b->set_parent(Object3DPtr());
    BOOST_CHECK(a->children.empty());
    BOOST_CHECK(!b->parent());

    // a -> b -> c
    c->set_parent(b);
    a->add_child(b);
    BOOST_CHECK(c->parent() == b);
    BOOST_CHECK(b->parent() == a);

    a->transform.translation = {0, 100, 0};
    b->transform.translation = {0, 50, 0};

    BOOST_CHECK(b->global_transform().translation == glm::dvec3(0, 150, 0));
}

BOOST_AUTO_TEST_CASE(test_Object3D_scene_entity)
{
    auto registry = std::make_shared<entt::registry>();
    Object3DPtr a(Object3D::create(registry)), b(Object3D::create(registry)), c(Object3D::create(registry));

    struct foo_component_t
    {
        int a = 0, b = 0;
    };

    // miss-case
    BOOST_CHECK(!c->has_component<foo_component_t>());

    // emplace new instance
    a->add_component<foo_component_t>();
    BOOST_CHECK(a->has_component<foo_component_t>());

    // copy existing
    foo_component_t foo_comp = {1, 2};
    b->add_component(foo_comp);
    BOOST_CHECK(b->has_component<foo_component_t>());

    auto &foo_ref = b->get_component<foo_component_t>();
    BOOST_CHECK_EQUAL(foo_ref.a, foo_comp.a);
    BOOST_CHECK_EQUAL(foo_ref.b, foo_comp.b);

    auto view = registry->view<vierkant::Object3D*, foo_component_t>();

    std::set<vierkant::Object3D*> foo_objects;
    for(auto [entity, object, foo]: view.each()){ foo_objects.insert(object); }
    BOOST_CHECK_EQUAL(foo_objects.size(), 2);
    foo_objects.contains(a.get());
    foo_objects.contains(b.get());

    // destruction
    bool destructed = false;
    {
        auto d = Object3D::create(registry);

        struct destruction_test_comp_t
        {
            std::function<void()> f;

            ~destruction_test_comp_t(){ if(f){ f(); }}
        };

        auto &destruct_comp = d->add_component<destruction_test_comp_t>();
        destruct_comp.f = [&destructed](){ destructed = true; };
    }
    BOOST_CHECK(destructed);
}
