#pragma once

#include <vierkant/Mesh.hpp>
#include <vierkant/Rasterizer.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

DEFINE_NAMED_ID(CollisionShapeId)
DEFINE_NAMED_ID(RigidBodyId)
DEFINE_NAMED_ID(SoftBodyId)
DEFINE_NAMED_ID(ConstraintId)

struct physics_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    CollisionShapeId shape_id = CollisionShapeId::nil();
    float mass = 0.f;
    bool kinematic = false;

    // TODO: contact callbacks
};

class PhysicsContext
{
public:
    PhysicsContext();

    PhysicsContext(PhysicsContext &&other) noexcept;

    PhysicsContext(const PhysicsContext &) = delete;

    PhysicsContext &operator=(PhysicsContext other);

    void step_simulation(float timestep, int max_sub_steps = 0, float fixed_time_step = 0.f);

    vierkant::GeometryPtr debug_render();

    RigidBodyId add_object(const vierkant::Object3DPtr &obj);
    void remove_object(const vierkant::Object3DPtr &obj);

    CollisionShapeId create_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                            const glm::vec3 &scale = glm::vec3(1));

    CollisionShapeId create_convex_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                                   const glm::vec3 &scale = glm::vec3(1));

    CollisionShapeId create_box_shape(const glm::vec3 &half_extents);
    CollisionShapeId create_plane_shape(const vierkant::Plane &plane);
    CollisionShapeId create_capsule_shape(float radius, float height);
    CollisionShapeId create_cylinder_shape(const glm::vec3 &half_extents);

private:
    struct engine;
    std::unique_ptr<engine, std::function<void(engine*)>> m_engine;
};

}//namespace vierkant
