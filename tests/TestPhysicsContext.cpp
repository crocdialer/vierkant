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
}

TEST(PhysicsContext, add_object)
{
    PhysicsContext context;
    auto box = Geometry::Box();
    auto collision_shape = create_collision_shape(context, box, true);

    auto scene = vierkant::Scene::create();
    Object3DPtr a(Object3D::create(scene->registry())), b(Object3D::create(scene->registry())),
            c(Object3D::create(scene->registry()));

    vierkant::physics_component_t phys_cmp = {};
    phys_cmp.mass = 1.f;
    phys_cmp.shape_id = collision_shape;

    Object3DPtr objects[] = {a, b, c};
    float i = 1;
    for(const auto &obj : objects)
    {
        obj->transform.translation.y = i++ * 5.f;

        obj->add_component(phys_cmp);
        auto body_id = context.add_object(obj);
        EXPECT_TRUE(body_id);
        scene->add_object(obj);
    }

    for(uint32_t l = 0; l < 1000; ++l)
    {
        context.step_simulation(1.f / 60.f, 0);
    }
    // all bodies should be flat on the ground here
    EXPECT_LE(a->transform.translation.y, 1.f);
    EXPECT_LE(b->transform.translation.y, 1.f);
    EXPECT_LE(c->transform.translation.y, 1.f);
}
