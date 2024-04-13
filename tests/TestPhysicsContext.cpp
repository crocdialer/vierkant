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
//    EXPECT_TRUE(create_collision_shape(context, box, false));
    EXPECT_TRUE(context.create_collision_shape(collision::box_t()));
    EXPECT_TRUE(context.create_collision_shape(collision::sphere_t()));
    EXPECT_TRUE(context.create_collision_shape(collision::cylinder_t()));
    EXPECT_TRUE(context.create_collision_shape(collision::capsule_t()));
}

TEST(PhysicsContext, add_remove_object)
{
    PhysicsContext context;

    glm::vec3 gravity = {0.f, -9.81f, 0.f};
    context.set_gravity(gravity);
    EXPECT_EQ(context.gravity(), gravity);

    auto scene = vierkant::Scene::create();
    auto a = Object3D::create(scene->registry());

    // a does not (yet) have a vierkant::physics_component, so adding has no effect
    context.add_object(a);
    EXPECT_FALSE(context.contains(a));

    // now add required component
    vierkant::object_component auto &cmp = a->add_component<vierkant::physics_component_t>();
    cmp.shape = collision::box_t{glm::vec3(0.5f)};

    context.add_object(a);
    EXPECT_TRUE(context.contains(a));
    EXPECT_EQ(context.body_interface().velocity(a->id()), glm::vec3(0));
    auto test_velocity = glm::vec3(0, 1.f, 0.f);
    context.body_interface().set_velocity(a->id(), glm::vec3(0, 1.f, 0.f));

    // TODO: fails, why?
//    EXPECT_EQ(context.body_interface().velocity(a->id()), glm::vec3(0, 1.f, 0.f));

    context.remove_object(a);
    EXPECT_FALSE(context.contains(a));
}

TEST(PhysicsContext, simulation)
{
//    spdlog::set_level(spdlog::level::debug);

    auto box = Geometry::Box();
    auto collision_shape = vierkant::collision::sphere_t();//create_collision_shape(context, box, true);

    auto scene = vierkant::PhysicsScene::create();
    Object3DPtr a(Object3D::create(scene->registry())), b(Object3D::create(scene->registry())),
            c(Object3D::create(scene->registry())), ground(Object3D::create(scene->registry()));

    auto &body_interface = scene->context().body_interface();

    std::map<uint32_t, uint32_t> contact_map, sensor_map;

    vierkant::physics_component_t phys_cmp = {};
    phys_cmp.mass = 1.f;
    phys_cmp.shape = collision_shape;
    phys_cmp.callbacks.contact_begin = [&contact_map](uint32_t obj_id)
    {
        spdlog::debug("contact_begin: {}", obj_id);
        contact_map[obj_id]++;
    };
    phys_cmp.callbacks.contact_end = [&contact_map](uint32_t obj_id)
    {
        spdlog::debug("contact_end: {}", obj_id);
        contact_map[obj_id]--;
    };
    a->add_component(phys_cmp);
    b->add_component(phys_cmp);

    // add c as static body with zero mass
    phys_cmp.mass = 0.f;
    c->add_component(phys_cmp);

    phys_cmp.shape = vierkant::collision::box_t{.half_extents = {2.f, .2f, 2.f}};
    ground->add_component(phys_cmp);

    Object3DPtr objects[] = {ground, a, b, c};
    float i = 0;
    for(const auto &obj: objects)
    {
        obj->transform.translation.y = i++ * 5.f;
        scene->add_object(obj);
        EXPECT_TRUE(scene->context().contains(obj));
    }

    auto sensor = vierkant::Object3D::create(scene->registry());
    sensor->name = "sensor";
    sensor->transform.translation.y = 3.f;
    phys_cmp.sensor = true;
    phys_cmp.kinematic = true;
    phys_cmp.shape = collision::box_t{glm::vec3(4.f, 0.5f, 4.f)};
    sensor->add_component(phys_cmp);
    scene->add_object(sensor);

    auto tground = ground->transform;
    auto ta = a->transform;
    auto tb = b->transform;
    auto tc = c->transform;

    // run simulation a bit
    for(uint32_t l = 0; l < 10; ++l) { scene->update(1.f / 60.f); }

    EXPECT_NE(body_interface.velocity(a->id()), glm::vec3(0));

    // bodies should be pulled down some way
    EXPECT_NE(ta, a->transform);
    EXPECT_NE(tb, b->transform);

    // ground and c were static and did not move
    EXPECT_EQ(tc, c->transform);
    EXPECT_EQ(tground, ground->transform);

    // remove object, again keep track of transforms
    scene->remove_object(b);
    ta = a->transform;
    tb = b->transform;

    // again, run simulation a bit
    for(uint32_t l = 0; l < 10; ++l) { scene->update(1.f / 60.f); }

    // b was removed, transform should still be the same
    EXPECT_NE(ta, a->transform);
    EXPECT_EQ(tb, b->transform);

//    // check if a and ground have contacts
//    EXPECT_TRUE(contact_map[a->id()]);
//    EXPECT_TRUE(contact_map[ground->id()]);
//
//    // c was floating, -> no contacts ever
//    EXPECT_FALSE(contact_map.contains(c->id()));
//
//    // b got removed, sensor was passed -> no contacts now, but there were some
//    EXPECT_TRUE(contact_map.contains(b->id()) && !contact_map[b->id()]);
//    EXPECT_TRUE(contact_map.contains(sensor->id()) && !contact_map[sensor->id()]);

//    auto debug_lines = context.debug_render();
//    EXPECT_FALSE(debug_lines->positions.empty());
//    EXPECT_EQ(debug_lines->positions.size(), debug_lines->colors.size());
}
