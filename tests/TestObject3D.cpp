#include <gtest/gtest.h>
#include "vierkant/Object3D.hpp"
#include "vierkant/Scene.hpp"

using namespace vierkant;
//____________________________________________________________________________//

TEST(Object3D, hierarchy)
{
    Object3DPtr a(Object3D::create()), b(Object3D::create()), c(Object3D::create());

    a->set_parent(b);
    EXPECT_TRUE(a->parent() == b);
    EXPECT_TRUE(b->children.size() == 1);

    b->remove_child(a);
    EXPECT_TRUE(!a->parent());

    b->remove_child(a);

    a->add_child(b);
    EXPECT_TRUE(a->children.size() == 1);
    EXPECT_TRUE(b->parent() == a);

    b->set_parent(Object3DPtr());
    EXPECT_TRUE(a->children.empty());
    EXPECT_TRUE(!b->parent());

    // a -> b -> c
    c->set_parent(b);
    a->add_child(b);
    EXPECT_TRUE(c->parent() == b);
    EXPECT_TRUE(b->parent() == a);

    a->transform.translation = {0, 100, 0};
    b->transform.translation = {0, 50, 0};
    EXPECT_TRUE(glm::vec3(b->global_transform().translation) == glm::vec3(0, 150, 0));

    vierkant::transform_t t = {};
    t.translation = {1, 2, 3};
    t.rotation = glm::angleAxis(glm::quarter_pi<float>(), glm::vec3(0, 1, 0));
    t.scale = glm::vec3(.5f);
    b->set_global_transform(t);
    EXPECT_TRUE(glm::vec3(b->global_transform().translation) == glm::vec3(1, 2, 3));
}

struct foo_component_t
{
    //! need to satisfy object_component concept
    static constexpr char component_description[] = "some foo test-component";

    int a = 0, b = 0;
};

struct destruction_test_comp_t
{
    //! need to satisfy object_component concept
    static constexpr char component_description[] = "some foo test-component testing component destruction";

    std::function<void()> f;

    ~destruction_test_comp_t(){ if(f){ f(); }}
};

TEST(Object3D, entity)
{
    auto registry = std::make_shared<entt::registry>();
    Object3DPtr a(Object3D::create(registry)), b(Object3D::create(registry)), c(Object3D::create(registry));

    // miss-case
    EXPECT_TRUE(!c->has_component<foo_component_t>());

    // emplace new instance
    a->add_component<foo_component_t>();
    EXPECT_TRUE(a->has_component<foo_component_t>());

    // copy existing
    foo_component_t foo_comp = {1, 2};
    b->add_component(foo_comp);
    EXPECT_TRUE(b->has_component<foo_component_t>());

    auto &foo_ref = b->get_component<foo_component_t>();
    EXPECT_EQ(foo_ref.a, foo_comp.a);
    EXPECT_EQ(foo_ref.b, foo_comp.b);

    auto view = registry->view<vierkant::Object3D*, foo_component_t>();

    std::set<vierkant::Object3D*> foo_objects;
    for(auto [entity, object, foo]: view.each()){ foo_objects.insert(object); }
    EXPECT_EQ(foo_objects.size(), 2);
    EXPECT_TRUE(foo_objects.contains(a.get()));
    EXPECT_TRUE(foo_objects.contains(b.get()));

    // destruction
    bool destructed = false;
    {
        auto d = Object3D::create(registry);

        auto &destruct_comp = d->add_component<destruction_test_comp_t>();
        destruct_comp.f = [&destructed](){ destructed = true; };
    }
    EXPECT_TRUE(destructed);
}
