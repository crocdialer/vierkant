#include <gtest/gtest.h>
#include <vierkant/Object3D.hpp>
#include <vierkant/Scene.hpp>
#include <vierkant/physics_context.hpp>

using namespace vierkant;
//____________________________________________________________________________//

CollisionShapeId create_collision_shape(PhysicsContext &context, const vierkant::GeometryPtr &geom, bool convex = true)
{
    Mesh::entry_create_info_t entry_create_info = {};
    entry_create_info.geometry = geom;
    auto mesh_bundle = vierkant::create_mesh_buffers({entry_create_info}, {});
    CollisionShapeId shape_id = CollisionShapeId::nil();
    shape_id =
            convex ? context.create_convex_collision_shape(mesh_bundle) : context.create_collision_shape(mesh_bundle);
    EXPECT_TRUE(shape_id);
    return shape_id;
}

TEST(PhysicsContext, collision_shapes)
{
    PhysicsContext context;
    auto box = Geometry::Box();
    EXPECT_TRUE(create_collision_shape(context, box, true));
    EXPECT_TRUE(create_collision_shape(context, box, false));
    EXPECT_TRUE(context.create_box_shape(glm::vec3(0.5f)));
    EXPECT_TRUE(context.create_plane_shape({}));
    EXPECT_TRUE(context.create_capsule_shape(.5f, 1.f));
    EXPECT_TRUE(context.create_cylinder_shape(glm::vec3(0.5f, 1.f, 0.5f)));
}

TEST(PhysicsContext, add_object)
{
    spdlog::set_level(spdlog::level::debug);
    PhysicsContext context;
    auto box = Geometry::Box();
    auto collision_shape = create_collision_shape(context, box, true);

    auto scene = vierkant::Scene::create();
    Object3DPtr a(Object3D::create(scene->registry())), b(Object3D::create(scene->registry())),
            c(Object3D::create(scene->registry())), ground(Object3D::create(scene->registry()));

    std::map<uint32_t, bool> trigger_map;

    vierkant::physics_component_t phys_cmp = {};
    phys_cmp.mass = 1.f;
    phys_cmp.shape_id = collision_shape;
    phys_cmp.callbacks.contact_begin = [&trigger_map](uint32_t obj_id)
    {
        spdlog::debug("contact_begin: {}", obj_id);
        trigger_map[obj_id] = true;
    };
    phys_cmp.callbacks.contact_end = [&trigger_map](uint32_t obj_id)
    {
        spdlog::debug("contact_end: {}", obj_id);
        trigger_map[obj_id] = true;
    };
    a->add_component(phys_cmp);
    b->add_component(phys_cmp);

    // add c as static body with zero mass
    phys_cmp.mass = 0.f;
    c->add_component(phys_cmp);

    phys_cmp.shape_id = context.create_plane_shape({});
    phys_cmp.collision_only = true;
//    phys_cmp.callbacks.collision = [&trigger_map](uint32_t obj_id) { trigger_map[obj_id] = true; };
    ground->add_component(phys_cmp);

    Object3DPtr objects[] = {ground, a, b, c};
    float i = 0;
    for(const auto &obj: objects)
    {
        obj->transform.translation.y = i++ * 5.f;
        auto body_id = context.add_object(obj);
        EXPECT_TRUE(body_id);
        scene->add_object(obj);
    }
    auto tground = ground->transform;
    auto ta = a->transform;
    auto tb = b->transform;
    auto tc = c->transform;

    // run simulation a bit
    for(uint32_t l = 0; l < 100; ++l) { context.step_simulation(1.f / 60.f); }

    // bodies should be pulled down some way
    EXPECT_NE(ta, a->transform);
    EXPECT_NE(tb, b->transform);

    // ground and c were static and did not move
    EXPECT_EQ(tc, c->transform);
    EXPECT_EQ(tground, ground->transform);

    // remove object, again keep track of transforms
    context.remove_object(b);
    ta = a->transform;
    tb = b->transform;

    // again, run simulation a bit
    for(uint32_t l = 0; l < 100; ++l) { context.step_simulation(1.f / 60.f); }

    // b was removed, transform should still be the same
    EXPECT_NE(ta, a->transform);
    EXPECT_EQ(tb, b->transform);

    // check if callbacks did fire
    EXPECT_TRUE(trigger_map[a->id()]);
    EXPECT_TRUE(trigger_map[b->id()]);
    EXPECT_TRUE(trigger_map[ground->id()]);
    EXPECT_FALSE(trigger_map[c->id()]);
}
