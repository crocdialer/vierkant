#include "vierkant/Object3D.hpp"
#include <gtest/gtest.h>
#include <random>
#include <ranges>

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

    a->set_transform({.translation = {0, 100, 0}});
    b->set_transform({.translation{0, 50, 0}});
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

    child->set_transform(
            transform_t{.translation = {0.1f, -0.03f, 7.25f},
                        .rotation = glm::angleAxis(glm::radians(123.456f), glm::normalize(glm::vec3(4, -7, 6))),
                        .scale = {0.37f, 1.23f, 2.91f}});

    // a present identity-transform must not perturb the accumulated transform ...
    group->set_transform({});
    EXPECT_TRUE(child->global_transform() == *child->transform());

    // ... and needs to match the absent-transform case exactly
    group->remove_transform();
    EXPECT_TRUE(child->global_transform() == *child->transform());
}

//! reference: the bottom-up ancestor-walk that global_transform() used to be. right-associated.
transform_t reference_global_bottom_up(const Object3D *object)
{
    transform_t ret = object->transform() ? *object->transform() : transform_t{};
    const Object3D *ancestor = object->parent();
    while(ancestor)
    {
        if(ancestor->transform()) { ret = *ancestor->transform() * ret; }
        ancestor = ancestor->parent();
    }
    return ret;
}

//! reference: root-first accumulation, as CullVisitor's transform-stack did it. left-associated.
transform_t reference_global_top_down(const Object3D *object)
{
    std::vector<const Object3D *> chain;
    for(const Object3D *o = object; o; o = o->parent()) { chain.push_back(o); }

    transform_t ret = {};
    for(const auto *o: std::ranges::reverse_view(chain))
    {
        if(o->transform()) { ret = ret * *o->transform(); }
    }
    return ret;
}

TEST(Object3D, global_transform_cache_same_frame_write)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr parent(object_store->create_object()), child(object_store->create_object());
    parent->add_child(child);

    parent->set_transform({.translation = {0.f, 10.f, 0.f}});
    child->set_transform({.translation = {0.f, 1.f, 0.f}});

    // prime both caches
    EXPECT_EQ(glm::vec3(child->global_transform().translation), glm::vec3(0.f, 11.f, 0.f));

    // a write to the parent must be visible to the child immediately, not only after a frame-boundary.
    // this is the physics_context.cpp:1770-1774 case: parents are written before children are read.
    parent->set_transform({.translation = {0.f, 20.f, 0.f}});
    EXPECT_EQ(glm::vec3(child->global_transform().translation), glm::vec3(0.f, 21.f, 0.f));

    // same via set_global_transform, which physics actually uses
    parent->set_global_transform(transform_t{.translation = {0.f, 30.f, 0.f}});
    EXPECT_EQ(glm::vec3(child->global_transform().translation), glm::vec3(0.f, 31.f, 0.f));
}

TEST(Object3D, global_transform_cache_invalidation)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr parent(object_store->create_object()), child(object_store->create_object()),
            grand_child(object_store->create_object());

    parent->set_transform({.translation = {0.f, 10.f, 0.f}});
    child->set_transform({.translation = {0.f, 1.f, 0.f}});
    grand_child->set_transform({.translation = {0.f, 0.1f, 0.f}});

    // invalidation has to reach descendants, not just direct children
    parent->add_child(child);
    child->add_child(grand_child);
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 11.1f, 0.f));

    parent->set_transform({.translation = {0.f, 20.f, 0.f}});
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 21.1f, 0.f));

    // remove_child drops an ancestor-chain
    parent->remove_child(child);
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 1.1f, 0.f));

    // set_parent re-attaches it
    child->set_parent(parent);
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 21.1f, 0.f));

    // set_parent(nullptr) detaches
    child->set_parent(Object3DPtr());
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 1.1f, 0.f));

    // removing the transform entirely
    child->remove_transform();
    EXPECT_TRUE(!child->transform());
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 0.1f, 0.f));
}

TEST(Object3D, global_transform_cache_clone)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr parent(object_store->create_object()), child(object_store->create_object());
    parent->set_transform({.translation = {0.f, 10.f, 0.f}});
    child->set_transform({.translation = {0.f, 1.f, 0.f}});
    parent->add_child(child);

    // prime the caches, so a clone would inherit stale values
    EXPECT_EQ(glm::vec3(child->global_transform().translation), glm::vec3(0.f, 11.f, 0.f));

    // the clone has no parent, so its global must equal its local
    auto clone = object_store->clone(child.get());
    EXPECT_TRUE(!clone->parent());
    EXPECT_EQ(glm::vec3(clone->global_transform().translation), glm::vec3(0.f, 1.f, 0.f));
}

//! chain of 'num_objects', every 3rd without a transform. scales are uniform, see the shear-note below.
std::vector<Object3DPtr> build_chain(vierkant::ObjectStore &object_store, uint32_t num_objects, bool uniform_scale)
{
    std::vector<Object3DPtr> objects;
    for(uint32_t i = 0; i < num_objects; ++i)
    {
        auto object = object_store.create_object();
        if(i % 3 != 0)
        {
            object->set_transform({.translation = {float(i), 2.f * float(i), -float(i)},
                                              .rotation = glm::angleAxis(glm::radians(17.f * float(i)),
                                                                         glm::normalize(glm::vec3(1, 2, 3))),
                                              .scale = uniform_scale
                                                               ? glm::vec3(0.5f + 0.1f * float(i))
                                                               : glm::vec3(1.f, 0.5f + 0.1f * float(i), 1.3f)});
        }
        if(i) { objects[i - 1]->add_child(object); }
        objects.push_back(object);
    }
    return objects;
}

TEST(Object3D, global_transform_cache_equivalence)
{
    auto object_store = vierkant::create_object_store();
    auto objects = build_chain(*object_store, 6, true);

    auto check_all = [&objects]() {
        for(const auto &object: objects)
        {
            EXPECT_TRUE(epsilon_equal(object->global_transform(), reference_global_bottom_up(object.get()), 1.e-5f));
        }
    };
    check_all();

    // a deterministic sequence of mutations, comparing against the reference-walk after each
    std::mt19937 rng(42);
    for(uint32_t i = 0; i < 64; ++i)
    {
        auto &object = objects[rng() % objects.size()];

        switch(rng() % 3)
        {
            case 0:
                object->set_transform({.translation = glm::vec3(float(rng() % 10), 1.f, 2.f),
                                                  .scale = glm::vec3(1.f + float(rng() % 3))});
                break;
            case 1: object->remove_transform(); break;
            case 2:
                if(object->parent()) { object->set_global_transform(transform_t{.translation = {1.f, 2.f, 3.f}}); }
                break;
            default: break;
        }
        check_all();
    }
}

/**
 * a cached global is 'parent_global * local', which accumulates root-first (left-associated).
 * the old bottom-up walk accumulated leaf-first (right-associated). those agree exactly in
 * exact arithmetic, but operator* projects shear away for non-uniform scale (transform.hpp:104),
 * and that projection is not associative - the two disagree by a lot on a sheared chain.
 *
 * this is not a regression: CullVisitor's transform-stack already accumulated root-first, so the
 * cached value matches what the renderer has always used, and global_transform() now agrees with it.
 */
TEST(Object3D, global_transform_cache_matches_top_down_accumulation)
{
    auto object_store = vierkant::create_object_store();
    auto objects = build_chain(*object_store, 6, false);

    for(const auto &object: objects)
    {
        EXPECT_TRUE(epsilon_equal(object->global_transform(), reference_global_top_down(object.get()), 1.e-5f));
    }

    // ... and the sheared chain really does disagree with the old bottom-up walk, i.e. the test above
    // would fail here. guards against someone "fixing" the shear-projection and silently changing poses.
    const auto &leaf = objects.back();
    EXPECT_FALSE(epsilon_equal(leaf->global_transform(), reference_global_bottom_up(leaf.get()), 1.e-5f));
}

TEST(Object3D, outliving_parent)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr child = object_store->create_object();
    child->set_transform({.translation = {1.f, 2.f, 3.f}});

    {
        Object3DPtr parent = object_store->create_object();
        parent->set_transform({.translation = {0.f, 50.f, 0.f}});
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