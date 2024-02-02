#pragma once

#include <vierkant/Mesh.hpp>
#include <vierkant/Rasterizer.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

DEFINE_NAMED_ID(CollisionShapeId)
DEFINE_NAMED_ID(RigiBodyId)
DEFINE_NAMED_ID(SoftBodyId)

struct physics_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    CollisionShapeId shape_id = CollisionShapeId::nil();
    float mass = 0.f;
};

class PhysicsContext
{
public:
    PhysicsContext();

    PhysicsContext(PhysicsContext &&other) noexcept;

    PhysicsContext(const PhysicsContext &) = delete;

    PhysicsContext &operator=(PhysicsContext other);

    void step_simulation(float timestep, int max_sub_steps = 1, float fixed_time_step = 0.f);

//    void debug_render(vierkant::Rasterizer &renderer, const vierkant::transform_t &transform,
//                      const glm::mat4 &projection);

    RigiBodyId add_object(const vierkant::Object3DPtr &obj);
    void remove_object(const vierkant::Object3DPtr &obj);

    CollisionShapeId create_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                            const glm::vec3 &scale = glm::vec3(1));

    CollisionShapeId create_convex_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                                   const glm::vec3 &scale = glm::vec3(1));

private:
    struct engine;
    std::unique_ptr<engine, std::function<void(engine*)>> m_engine;
};

}//namespace vierkant
