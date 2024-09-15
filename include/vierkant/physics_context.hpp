#pragma once

#include <crocore/NamedId.hpp>
#include <crocore/NamedUUID.hpp>
#include <crocore/ThreadPool.hpp>

#include <variant>
#include <vierkant/Mesh.hpp>
#include <vierkant/Scene.hpp>
#include <vierkant/intersection.hpp>
#include <vierkant/object_component.hpp>

namespace vierkant
{

DEFINE_NAMED_UUID(CollisionShapeId)

namespace collision
{

struct box_t
{
    glm::vec3 half_extents = glm::vec3(.5f);
    constexpr bool operator==(const vierkant::collision::box_t &other) const = default;
};

struct sphere_t
{
    float radius = 1.f;
    constexpr bool operator==(const vierkant::collision::sphere_t &other) const = default;
};

struct cylinder_t
{
    float radius = 1.f;
    float height = 1.f;
    constexpr bool operator==(const vierkant::collision::cylinder_t &other) const = default;
};

struct capsule_t
{
    float radius = 1.f;
    float height = 1.f;
    constexpr bool operator==(const vierkant::collision::capsule_t &other) const = default;
};

struct mesh_t
{
    vierkant::MeshId mesh_id = vierkant::MeshId::nil();
    bool convex_hull = true;
    constexpr bool operator==(const vierkant::collision::mesh_t &other) const = default;
};

using shape_t = std::variant<collision::sphere_t, collision::box_t, collision::cylinder_t, collision::capsule_t,
                             vierkant::CollisionShapeId>;
}// namespace collision

struct physics_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    collision::shape_t shape = CollisionShapeId::nil();
    float mass = 0.f;
    float friction = 0.2f;
    float restitution = 0.f;
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    bool kinematic = false;
    bool sensor = false;

    bool need_update = false;

    constexpr bool operator==(const vierkant::physics_component_t &) const = default;
};

class PhysicsContext
{
public:
    struct callbacks_t
    {
        using contact_cb_t = std::function<void(uint32_t, uint32_t)>;
        contact_cb_t collision;
        contact_cb_t contact_begin;
        contact_cb_t contact_end;
    };

    class BodyInterface
    {
    public:
        virtual bool get_transform(uint32_t objectId, vierkant::transform_t &t) const = 0;
        virtual void set_transform(uint32_t objectId, const vierkant::transform_t &t) const = 0;
        virtual void add_force(uint32_t objectId, const glm::vec3 &force, const glm::vec3 &offset) = 0;
        virtual void add_impulse(uint32_t objectId, const glm::vec3 &impulse, const glm::vec3 &offset) = 0;
        [[nodiscard]] virtual glm::vec3 velocity(uint32_t objectId) const = 0;
        virtual void set_velocity(uint32_t objectId, const glm::vec3 &velocity) = 0;
        virtual void activate(uint32_t objectId) = 0;
        virtual void activate_in_aabb(const vierkant::AABB &aabb) = 0;
        virtual bool is_active(uint32_t objectId) = 0;
    };

    explicit PhysicsContext(crocore::ThreadPool *thread_pool = nullptr);

    PhysicsContext(PhysicsContext &&other) noexcept;

    PhysicsContext(const PhysicsContext &) = delete;

    PhysicsContext &operator=(PhysicsContext other);

    void step_simulation(float timestep, int max_sub_steps = 1);

    GeometryConstPtr debug_render();

    void set_gravity(const glm::vec3 &g);
    [[nodiscard]] glm::vec3 gravity() const;

    void add_object(uint32_t objectId, const vierkant::transform_t &transform,
                    const vierkant::physics_component_t &cmp);
    void remove_object(uint32_t objectId);
    [[nodiscard]] bool contains(uint32_t objectId) const;

    void set_callbacks(uint32_t objectId, const callbacks_t &callbacks);

    void set_threadpool(crocore::ThreadPool &pool);

    BodyInterface &body_interface();

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

    vierkant::PhysicsContext &physics_context() { return m_context; };

private:
    explicit PhysicsScene() = default;

    crocore::ThreadPool m_thread_pool{std::thread::hardware_concurrency() - 1};
    vierkant::PhysicsContext m_context{&m_thread_pool};
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