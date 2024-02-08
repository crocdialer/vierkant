#pragma once

#include <crocore/NamedId.hpp>
#include <crocore/NamedUUID.hpp>
#include <variant>
#include <vierkant/Mesh.hpp>
#include <vierkant/Scene.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

DEFINE_NAMED_UUID(CollisionShapeId)
DEFINE_NAMED_ID(RigidBodyId)
DEFINE_NAMED_ID(SoftBodyId)
DEFINE_NAMED_ID(ConstraintId)

namespace collision
{

struct plane_t
{
    glm::vec3 normal = glm::vec3(0, 1, 0);
    float d = 0;
};

struct box_t
{
    glm::vec3 half_extents = glm::vec3(.5f);
};

struct sphere_t
{
    float radius = 1.f;
};

struct cylinder_t
{
    glm::vec3 half_extents = glm::vec3(.5f);
};

struct capsule_t
{
    float radius = 1.f;
    float height = 1.f;
};

struct mesh_t
{
    //    vierkant::MeshId mesh_id = vierkant::MeshId::nil();
    bool convex_hull = true;
};

using shape_t = std::variant<collision::plane_t, collision::sphere_t, collision::box_t, collision::cylinder_t,
                             collision::capsule_t, vierkant::CollisionShapeId>;
}// namespace collision

using collision_cb_t = std::function<void(uint32_t)>;
struct physics_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    collision::shape_t shape = CollisionShapeId::nil();
    float mass = 0.f;
    float friction = 0.5f;
    float rolling_friction = 0.0f;
    float spinning_friction = 0.0f;
    float restitution = 0.f;
    bool kinematic = false;
    bool sensor = false;

    struct callbacks_t
    {
        collision_cb_t collision;
        collision_cb_t contact_begin;
        collision_cb_t contact_end;
    } callbacks;

    bool need_update = false;
};

bool operator==(const vierkant::physics_component_t &lhs, const vierkant::physics_component_t &rhs);
inline bool operator!=(const vierkant::physics_component_t &lhs, const vierkant::physics_component_t &rhs)
{
    return !(lhs == rhs);
}

class PhysicsContext
{
public:
    PhysicsContext();

    PhysicsContext(PhysicsContext &&other) noexcept;

    PhysicsContext(const PhysicsContext &) = delete;

    PhysicsContext &operator=(PhysicsContext other);

    void step_simulation(float timestep, int max_sub_steps = 1, float fixed_time_step = 1.f / 60.f);

    const GeometryPtr &debug_render();

    void set_gravity(const glm::vec3 &g);
    [[nodiscard]] glm::vec3 gravity() const;

    RigidBodyId add_object(const vierkant::Object3DPtr &obj);
    void remove_object(const vierkant::Object3DPtr &obj);
    [[nodiscard]] bool contains(const vierkant::Object3DPtr &obj) const;
    [[nodiscard]] RigidBodyId body_id(const vierkant::Object3DPtr &obj) const;

    void apply_force(const vierkant::Object3DPtr &obj, const glm::vec3 &force, const glm::vec3 &offset = {});
    void apply_impulse(const vierkant::Object3DPtr &obj, const glm::vec3 &impulse, const glm::vec3 &offset = {});
    glm::vec3 velocity(const vierkant::Object3DPtr &obj);
    void set_velocity(const vierkant::Object3DPtr &obj, const glm::vec3 &velocity);

    CollisionShapeId create_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                            const glm::vec3 &scale = glm::vec3(1));

    CollisionShapeId create_convex_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                                   const glm::vec3 &scale = glm::vec3(1));

    CollisionShapeId create_collision_shape(const collision::shape_t &shape);

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

    vierkant::PhysicsContext &context() { return m_context; };

private:
    PhysicsScene() = default;
    vierkant::PhysicsContext m_context;
};

}//namespace vierkant

// template specializations for hashing
namespace std
{
template<>
struct hash<vierkant::physics_component_t>
{
    size_t operator()(vierkant::physics_component_t const &c) const;
};
}// namespace std