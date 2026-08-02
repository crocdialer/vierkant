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

//! set_global_transform(t) followed by global_transform() has to reproduce t, whatever the parent is
TEST(Object3D, set_global_transform_roundtrip)
{
    auto object_store = vierkant::create_object_store();

    const transform_t targets[] = {
            {.translation = {1.f, 2.f, 3.f}},
            {.translation = {-4.f, 0.5f, 9.f},
             .rotation = glm::angleAxis(glm::radians(73.f), glm::normalize(glm::vec3(1, -2, 4))),
             .scale = glm::vec3(2.f)},
            {.translation = {0.3f, -1.f, 0.f},
             .rotation = glm::angleAxis(glm::radians(31.f), glm::normalize(glm::vec3(0, 1, 1))),
             .scale = {0.4f, 1.7f, 2.2f}},
    };

    const transform_t parents[] = {
            {},
            {.translation = {5.f, 0.f, -2.f}},
            {.translation = {5.f, 0.f, -2.f},
             .rotation = glm::angleAxis(glm::radians(30.f), glm::vec3(0, 1, 0)),
             .scale = glm::vec3(3.f)},
            // non-uniform scale + rotation, i.e. a sheared parent-chain
            {.translation = {1.f, 2.f, 3.f},
             .rotation = glm::angleAxis(glm::radians(30.f), glm::vec3(0, 1, 0)),
             .scale = {1.f, 3.f, 1.f}},
    };

    for(const auto &parent_transform: parents)
    {
        for(const auto &target: targets)
        {
            Object3DPtr parent(object_store->create_object()), child(object_store->create_object());
            parent->set_transform(parent_transform);
            parent->add_child(child);

            child->set_global_transform(target);
            EXPECT_TRUE(epsilon_equal(child->global_transform(), target, 1.e-5f));
        }
    }
}

//! a fully absolute node's pose is its world-pose, its parent-chain is ignored entirely
TEST(Object3D, transform_space_absolute)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr parent(object_store->create_object()), other_parent(object_store->create_object()),
            child(object_store->create_object());

    parent->set_transform({.translation = {5.f, 0.f, -2.f},
                           .rotation = glm::angleAxis(glm::radians(30.f), glm::vec3(0, 1, 0)),
                           .scale = glm::vec3(3.f)});
    other_parent->set_transform({.translation = {-100.f, 7.f, 42.f}, .scale = glm::vec3(0.25f)});

    const transform_t world = {.translation = {1.f, 2.f, 3.f},
                               .rotation = glm::angleAxis(glm::radians(73.f), glm::normalize(glm::vec3(1, -2, 4))),
                               .scale = {0.4f, 1.7f, 2.2f}};
    child->set_transform(world);
    child->set_transform_space(transform_component_t::ABSOLUTE);

    parent->add_child(child);
    EXPECT_TRUE(child->global_transform() == world);

    // re-parenting an absolute node must not move it
    other_parent->add_child(child);
    EXPECT_TRUE(child->global_transform() == world);

    child->set_parent(Object3DPtr());
    EXPECT_TRUE(child->global_transform() == world);
}

//! translation/rotation absolute + relative scale: the shape a simulation-driven rigid body uses
TEST(Object3D, transform_space_per_channel)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr parent(object_store->create_object()), body(object_store->create_object());

    const transform_t parent_transform = {.translation = {5.f, 0.f, -2.f},
                                          .rotation = glm::angleAxis(glm::radians(30.f), glm::vec3(0, 1, 0)),
                                          .scale = glm::vec3(3.f)};
    parent->set_transform(parent_transform);
    parent->add_child(body);

    const transform_t stored = {.translation = {1.f, 2.f, 3.f},
                                .rotation = glm::angleAxis(glm::radians(73.f), glm::normalize(glm::vec3(1, -2, 4))),
                                .scale = glm::vec3(2.f)};
    body->set_transform(stored);
    body->set_transform_space(transform_component_t::ABSOLUTE_TRANSLATION |
                              transform_component_t::ABSOLUTE_ROTATION);

    const auto global = body->global_transform();

    // the absolute channels pass through untouched ...
    EXPECT_EQ(glm::vec3(global.translation), glm::vec3(stored.translation));
    EXPECT_TRUE(glm::all(glm::epsilonEqual(glm::vec4(global.rotation.x, global.rotation.y, global.rotation.z,
                                                     global.rotation.w),
                                           glm::vec4(stored.rotation.x, stored.rotation.y, stored.rotation.z,
                                                     stored.rotation.w),
                                           1.e-6f)));

    // ... while the relative one still composes with the parent
    EXPECT_TRUE(glm::all(
            glm::epsilonEqual(global.scale, parent_transform.scale * stored.scale, 1.e-5f)));

    // relative_transform() undoes the parent again, so aabb() and friends stay correct
    EXPECT_TRUE(epsilon_equal(parent->global_transform() * body->relative_transform(), global, 1.e-5f));
}

//! invalidation prunes at fully-absolute nodes only, partially-absolute ones still depend on the parent
TEST(Object3D, transform_space_invalidation)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr parent(object_store->create_object()), absolute(object_store->create_object()),
            partial(object_store->create_object()), grand_child(object_store->create_object());

    parent->set_transform({.translation = {0.f, 10.f, 0.f}});
    parent->add_child(absolute);
    parent->add_child(partial);
    absolute->add_child(grand_child);

    absolute->set_transform({.translation = {0.f, 1.f, 0.f}});
    absolute->set_transform_space(transform_component_t::ABSOLUTE);
    partial->set_transform({.translation = {0.f, 1.f, 0.f}, .scale = glm::vec3(2.f)});
    partial->set_transform_space(transform_component_t::ABSOLUTE_TRANSLATION);
    grand_child->set_transform({.translation = {0.f, 0.5f, 0.f}});

    EXPECT_EQ(glm::vec3(absolute->global_transform().translation), glm::vec3(0.f, 1.f, 0.f));
    EXPECT_EQ(glm::vec3(partial->global_transform().translation), glm::vec3(0.f, 1.f, 0.f));
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 1.5f, 0.f));

    // moving the parent leaves the absolute sub-tree exactly where it was ...
    parent->set_transform({.translation = {0.f, 20.f, 0.f}});
    EXPECT_EQ(glm::vec3(absolute->global_transform().translation), glm::vec3(0.f, 1.f, 0.f));
    EXPECT_EQ(glm::vec3(grand_child->global_transform().translation), glm::vec3(0.f, 1.5f, 0.f));

    // ... but the partially-absolute node's relative scale-channel must follow along
    parent->set_transform({.translation = {0.f, 20.f, 0.f}, .scale = glm::vec3(4.f)});
    EXPECT_EQ(glm::vec3(partial->global_transform().translation), glm::vec3(0.f, 1.f, 0.f));
    EXPECT_TRUE(glm::all(glm::epsilonEqual(partial->global_transform().scale, glm::vec3(8.f), 1.e-5f)));
}

constexpr uint8_t g_all_spaces[] = {
        transform_component_t::RELATIVE,
        transform_component_t::ABSOLUTE_TRANSLATION,
        transform_component_t::ABSOLUTE_ROTATION,
        transform_component_t::ABSOLUTE_SCALE,
        transform_component_t::ABSOLUTE_TRANSLATION | transform_component_t::ABSOLUTE_ROTATION,
        transform_component_t::ABSOLUTE};

transform_t roundtrip_target()
{
    return {.translation = {-4.f, 0.5f, 9.f},
            .rotation = glm::angleAxis(glm::radians(73.f), glm::normalize(glm::vec3(1, -2, 4))),
            .scale = {0.4f, 1.7f, 2.2f}};
}

//! set_global_transform(t) -> global_transform() has to reproduce t for mixed-space nodes too
TEST(Object3D, set_global_transform_roundtrip_per_channel)
{
    auto object_store = vierkant::create_object_store();

    const transform_t parents[] = {
            {},
            {.translation = {5.f, 0.f, -2.f}},
            {.translation = {5.f, 0.f, -2.f},
             .rotation = glm::angleAxis(glm::radians(30.f), glm::vec3(0, 1, 0)),
             .scale = glm::vec3(3.f)},
    };

    const auto target = roundtrip_target();

    for(const auto &parent_transform: parents)
    {
        for(uint8_t space: g_all_spaces)
        {
            Object3DPtr parent(object_store->create_object()), child(object_store->create_object());
            parent->set_transform(parent_transform);
            parent->add_child(child);

            child->set_transform({});
            child->set_transform_space(space);
            child->set_global_transform(target);
            EXPECT_TRUE(epsilon_equal(child->global_transform(), target, 1.e-5f));
        }
    }
}

/**
 * a sheared parent-chain (non-uniform scale + rotation) is outside the engine's no-shear invariant,
 * see transform.hpp. the pure-relative roundtrip still works there, because set_global_transform's
 * decompose and operator*'s cancel exactly (a QR pre-image). overriding the rotation-channel breaks
 * that cancellation: the pre-image's rotation is what the projected scale is derived from.
 *
 * measured: translation and rotation stay exact, the relative scale-channel drifts by ~0.79 on the
 * case below. there is no closed-form fix - P * L is a lossy projection, so 'solve L for a given
 * (P*L).scale' has no general solution. pinned here so nobody mistakes it for a regression.
 */
TEST(Object3D, set_global_transform_roundtrip_sheared_parent)
{
    auto object_store = vierkant::create_object_store();
    const transform_t sheared_parent = {.translation = {1.f, 2.f, 3.f},
                                        .rotation = glm::angleAxis(glm::radians(30.f), glm::vec3(0, 1, 0)),
                                        .scale = {1.f, 3.f, 1.f}};
    ASSERT_FALSE(is_scale_uniform(sheared_parent));
    const auto target = roundtrip_target();

    for(uint8_t space: g_all_spaces)
    {
        Object3DPtr parent(object_store->create_object()), child(object_store->create_object());
        parent->set_transform(sheared_parent);
        parent->add_child(child);

        child->set_transform({});
        child->set_transform_space(space);
        child->set_global_transform(target);
        const auto global = child->global_transform();

        // the absolute channels are stored verbatim, so they always come back exactly
        EXPECT_TRUE(glm::all(glm::epsilonEqual(global.translation, target.translation, 1.e-5f)));
        EXPECT_TRUE(std::abs(glm::dot(global.rotation, target.rotation)) > 1.f - 1.e-5f);

        // an absolute rotation is what breaks the QR cancellation, and it shows up in the scale
        const bool scale_survives = !(space & transform_component_t::ABSOLUTE_ROTATION) ||
                                    (space & transform_component_t::ABSOLUTE_SCALE);
        EXPECT_EQ(scale_survives,
                  glm::all(glm::epsilonEqual(global.scale, target.scale, 1.e-5f)));
    }
}

//! a child with absolute channels contributes its converted, parent-relative extent
TEST(Object3D, aabb_with_absolute_child)
{
    auto object_store = vierkant::create_object_store();
    Object3DPtr parent(object_store->create_object()), child(object_store->create_object());

    const AABB unit_box = {glm::vec3(-0.5f), glm::vec3(0.5f)};
    child->add_component<aabb_component_t>({.aabb_fn = [unit_box](const Object3D &) { return unit_box; }});

    parent->set_transform({.translation = {10.f, 0.f, 0.f}});
    parent->add_child(child);

    // the same world-pose, expressed relative and absolute, has to yield the same parent-space aabb
    child->set_transform({.translation = {-9.f, 0.f, 0.f}});
    const auto relative_aabb = parent->aabb();

    child->set_transform_space(transform_component_t::ABSOLUTE);
    child->set_transform({.translation = {1.f, 0.f, 0.f}});
    const auto absolute_aabb = parent->aabb();

    EXPECT_TRUE(glm::all(glm::epsilonEqual(relative_aabb.min, absolute_aabb.min, 1.e-5f)));
    EXPECT_TRUE(glm::all(glm::epsilonEqual(relative_aabb.max, absolute_aabb.max, 1.e-5f)));

    // and the world-space extent is the child's box around world-origin+1
    const auto world_aabb = parent->aabb().transform(parent->global_transform());
    EXPECT_TRUE(glm::all(glm::epsilonEqual(world_aabb.min, glm::vec3(0.5f, -0.5f, -0.5f), 1.e-5f)));
    EXPECT_TRUE(glm::all(glm::epsilonEqual(world_aabb.max, glm::vec3(1.5f, 0.5f, 0.5f), 1.e-5f)));
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