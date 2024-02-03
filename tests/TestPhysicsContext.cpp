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
    PhysicsContext context;
    auto box = Geometry::Box();
    auto collision_shape = create_collision_shape(context, box, true);

    auto scene = vierkant::Scene::create();
    Object3DPtr a(Object3D::create(scene->registry())), b(Object3D::create(scene->registry())),
            c(Object3D::create(scene->registry())), ground(Object3D::create(scene->registry()));

    vierkant::physics_component_t phys_cmp = {};
    phys_cmp.mass = 1.f;
    phys_cmp.shape_id = collision_shape;
    phys_cmp.contact_begin = [](uint32_t obj_id) { spdlog::info("contact_begin: {}", obj_id); };
    phys_cmp.contact_end = [](uint32_t obj_id) { spdlog::info("contact_end: {}", obj_id); };
    a->add_component(phys_cmp);
    b->add_component(phys_cmp);

    // add c as static body with zero mass
    phys_cmp.mass = 0.f;
    c->add_component(phys_cmp);

    bool ground_triggered = false;
    phys_cmp.shape_id = context.create_plane_shape({});
    phys_cmp.collision_cb = [&ground_triggered](uint32_t obj_id) { ground_triggered = true; };
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

    context.remove_object(b);
    ta = a->transform;
    tb = b->transform;

    // again, run simulation a bit
    for(uint32_t l = 0; l < 100; ++l) { context.step_simulation(1.f / 60.f); }

    // b was removed, transform should still be the same
    EXPECT_NE(ta, a->transform);
    EXPECT_EQ(tb, b->transform);

    EXPECT_TRUE(ground_triggered);
}
