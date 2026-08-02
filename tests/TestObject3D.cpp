#include "vierkant/Object3D.hpp"
#include <gtest/gtest.h>

using namespace vierkant;
//____________________________________________________________________________//

struct test_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();
    int a = 0, b = 0;
    bool operator==(const test_component_t &other) const { return a == other.a && b == other.b; }
};

TEST(Object3D, hierarchy)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr a(object_store->create_object()), b(object_store->create_object()), c(object_store->create_object());

    a->set_parent(b);
    EXPECT_TRUE(a->parent() == b.get());
    EXPECT_TRUE(b->children.size() == 1);

    b->remove_child(a);
    EXPECT_TRUE(!a->parent());

    b->remove_child(a);

    a->add_child(b);
    EXPECT_TRUE(a->children.size() == 1);
    EXPECT_TRUE(b->parent() == a.get());

    b->set_parent(Object3DPtr());
    EXPECT_TRUE(a->children.empty());
    EXPECT_TRUE(!b->parent());

    // a -> b -> c
    c->set_parent(b);
    a->add_child(b);
    EXPECT_TRUE(c->parent() == b.get());
    EXPECT_TRUE(b->parent() == a.get());

    a->transform = transform_t{.translation = {0, 100, 0}};
    b->transform = transform_t{.translation{0, 50, 0}};
    EXPECT_TRUE(glm::vec3(b->global_transform().translation) == glm::vec3(0, 150, 0));

    vierkant::transform_t t = {};
    t.translation = {1, 2, 3};
    t.rotation = glm::angleAxis(glm::quarter_pi<float>(), glm::vec3(0, 1, 0));
    t.scale = glm::vec3(.5f);
    b->set_global_transform(t);
    EXPECT_TRUE(glm::vec3(b->global_transform().translation) == glm::vec3(1, 2, 3));
}

TEST(Object3D, identity_grouping_node)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr root(object_store->create_object()), group(object_store->create_object()),
            child(object_store->create_object());

    root->add_child(group);
    group->add_child(child);

    child->transform =
            transform_t{.translation = {0.1f, -0.03f, 7.25f},
                        .rotation = glm::angleAxis(glm::radians(123.456f), glm::normalize(glm::vec3(4, -7, 6))),
                        .scale = {0.37f, 1.23f, 2.91f}};

    // a present identity-transform must not perturb the accumulated transform ...
    group->transform = transform_t{};
    EXPECT_TRUE(child->global_transform() == *child->transform);

    // ... and needs to match the absent-transform case exactly
    group->transform = {};
    EXPECT_TRUE(child->global_transform() == *child->transform);
}

TEST(Object3D, outliving_parent)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr child = object_store->create_object();
    child->transform = transform_t{.translation = {1.f, 2.f, 3.f}};

    {
        Object3DPtr parent = object_store->create_object();
        parent->transform = transform_t{.translation = {0.f, 50.f, 0.f}};
        parent->add_child(child);
        ASSERT_TRUE(child->parent() == parent.get());
        EXPECT_EQ(glm::vec3(child->global_transform().translation), glm::vec3(1.f, 52.f, 3.f));
    }

    // the child outlives its parent here, so its raw parent-pointer has to be cleared
    EXPECT_TRUE(!child->parent());
    EXPECT_EQ(glm::vec3(child->global_transform().translation), glm::vec3(1.f, 2.f, 3.f));
}

TEST(Object3D, entity)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr a(object_store->create_object()), b(object_store->create_object()), c(object_store->create_object());

    // miss-case
    EXPECT_TRUE(!c->has_component<test_component_t>());

    // emplace new instance
    a->add_component<test_component_t>();
    EXPECT_TRUE(a->has_component<test_component_t>());

    // copy existing
    test_component_t foo_comp = {1, 2};
    b->add_component(foo_comp);
    EXPECT_TRUE(b->has_component<test_component_t>());

    auto &foo_ref = b->get_component<test_component_t>();
    EXPECT_EQ(foo_ref.a, foo_comp.a);
    EXPECT_EQ(foo_ref.b, foo_comp.b);

    auto view = object_store->registry()->view<vierkant::Object3D *, test_component_t>();

    std::set<vierkant::Object3D *> foo_objects;
    for(auto [entity, object, foo]: view.each()) { foo_objects.insert(object); }
    EXPECT_EQ(foo_objects.size(), 2);
    EXPECT_TRUE(foo_objects.contains(a.get()));
    EXPECT_TRUE(foo_objects.contains(b.get()));

    // destruction
    bool destructed = false;
    {
        auto d = object_store->create_object();

        struct destruction_test_comp_t
        {
            VIERKANT_ENABLE_AS_COMPONENT();

            std::function<void()> f;

            ~destruction_test_comp_t()
            {
                if(f) { f(); }
            }
        };

        auto &destruct_comp = d->add_component<destruction_test_comp_t>();
        destruct_comp.f = [&destructed]() { destructed = true; };
    }
    EXPECT_TRUE(destructed);
}

TEST(Object3D, clone)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr a(object_store->create_object()), b(object_store->create_object());

    a->add_component<test_component_t>({1, 2});
    b->add_component<test_component_t>({3, 4});
    a->add_child(b);

    auto c = object_store->clone(a.get());

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_TRUE(c);

    EXPECT_NE(a, c);
    EXPECT_EQ(a->children.size(), c->children.size());

    EXPECT_TRUE(a->has_component<test_component_t>());
    EXPECT_EQ(a->get_component<test_component_t>(), c->get_component<test_component_t>());

    // components have truly been copied and are not references
    c->get_component<test_component_t>().a = 69;
    EXPECT_NE(a->get_component<test_component_t>(), c->get_component<test_component_t>());

    // test recursive component-cloning
    EXPECT_EQ(c->children.front()->get_component<test_component_t>(), b->get_component<test_component_t>());
}