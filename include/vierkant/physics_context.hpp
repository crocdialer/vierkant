#pragma once

#include <crocore/NamedId.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Scene.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

DEFINE_NAMED_ID(CollisionShapeId)
DEFINE_NAMED_ID(RigidBodyId)
DEFINE_NAMED_ID(SoftBodyId)
DEFINE_NAMED_ID(ConstraintId)

using collision_cb_t = std::function<void(uint32_t)>;
struct physics_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    CollisionShapeId shape_id = CollisionShapeId::nil();
    float mass = 0.f;
    float friction = 0.5f;
    float rolling_friction = 0.0f;
    float spinning_friction = 0.0f;
    float restitution = 0.f;
    bool kinematic = false;
    bool collision_only = false;

    struct callbacks_t
    {
        collision_cb_t collision;
        collision_cb_t contact_begin;
        collision_cb_t contact_end;
    } callbacks;
};

class PhysicsContext
{
public:
    PhysicsContext();

    PhysicsContext(PhysicsContext &&other) noexcept;

    PhysicsContext(const PhysicsContext &) = delete;

    PhysicsContext &operator=(PhysicsContext other);

    void step_simulation(float timestep, int max_sub_steps = 1, float fixed_time_step = 1.f / 60.f);

    const std::unordered_map<glm::vec4, std::vector<glm::vec3>>& debug_render();

    void set_gravity(const glm::vec3 &g);
    [[nodiscard]] glm::vec3 gravity() const;

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
    std::unique_ptr<engine, std::function<void(engine *)>> m_engine;
};


class PhysicsScene : public vierkant::Scene
{
public:

    ~PhysicsScene() override = default;

    static std::shared_ptr<PhysicsScene> create();

    void add_object(const Object3DPtr &object) override;

    void remove_object(const Object3DPtr &object) override;

    void clear() override;

    void update(double time_delta) override;

    vierkant::PhysicsContext& context(){ return m_context; };

private:
    PhysicsScene() = default;
    vierkant::PhysicsContext m_context;
};

}//namespace vierkant
