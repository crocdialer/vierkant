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

DEFINE_NAMED_UUID(BodyId)
DEFINE_NAMED_UUID(CollisionShapeId)
DEFINE_NAMED_UUID(ConstraintId)

namespace collision
{

struct none_t
{
    constexpr bool operator==(const vierkant::collision::none_t &other) const = default;
};

struct plane_t
{
    glm::vec4 coefficients = glm::vec4(0.f, 1.f, 0.f, 0.f);
    float half_extent = 1000.f;
    constexpr bool operator==(const vierkant::collision::plane_t &other) const = default;
};

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
    static constexpr uint32_t MAX_LOD_BIAS = std::numeric_limits<uint32_t>::max();

    vierkant::MeshId mesh_id = vierkant::MeshId::nil();

    //! optional set of used entry-indices.
    std::optional<std::unordered_set<uint32_t>> entry_indices = {};

    //! flag indicating that the mesh is used as mesh-library and entry-transforms should be skipped
    bool library = false;

    bool convex_hull = false;

    //! lod-bias, defaults to 0 (highest detail). use MAX_LOD_BIAS to always request max-lod (lowest detail)
    uint32_t lod_bias = 0;

    constexpr bool operator==(const vierkant::collision::mesh_t &other) const = default;
};

using mesh_provider_fn = std::function<vierkant::mesh_asset_t(vierkant::MeshId)>;

using shape_t = std::variant<vierkant::CollisionShapeId, collision::plane_t, collision::none_t, collision::sphere_t,
                             collision::box_t, collision::cylinder_t, collision::capsule_t, collision::mesh_t>;
}// namespace collision

namespace constraint
{

enum class ConstraintSpace
{
    LocalToBodyCOM,
    World,
};

enum class SpringMode : uint8_t
{
    FrequencyAndDamping,
    StiffnessAndDamping,
};

struct spring_settings_t
{
    SpringMode mode = SpringMode::FrequencyAndDamping;

    /// depending on mode = SpringMode::FrequencyAndDamping.
    /// If Frequency > 0 the constraint will be soft and mFrequency specifies the oscillation frequency in Hz.
    /// If Frequency <= 0, mDamping is ignored and the constraint will have hard limits (as hard as the time step / the number of velocity / position solver steps allows).
    float frequency_or_stiffness = 0.0f;

    /// When SpringMode = ESpringMode::FrequencyAndDamping mDamping is the damping ratio (0 = no damping, 1 = critical damping).
    /// When SpringMode = ESpringMode::StiffnessAndDamping mDamping is the damping (c) in the spring equation F = -k * x - c * v for a linear or T = -k * theta - c * w for an angular spring.
    /// Note that if you set mDamping = 0, you will not get an infinite oscillation. Because we integrate physics using an explicit Euler scheme, there is always energy loss.
    /// This is done to keep the simulation from exploding, because with a damping of 0 and even the slightest rounding error, the oscillation could become bigger and bigger until the simulation explodes.
    float damping = 0.0f;
    constexpr bool operator==(const vierkant::constraint::spring_settings_t &other) const = default;
};

struct none_t
{
    constexpr bool operator==(const vierkant::constraint::none_t &other) const = default;
};

struct point_t
{
    ConstraintSpace space = ConstraintSpace::World;
    glm::vec3 point1;
    glm::vec3 point2;
    constexpr bool operator==(const vierkant::constraint::point_t &other) const = default;
};

struct distance_t
{
    ConstraintSpace space = ConstraintSpace::World;
    glm::vec3 point1;
    glm::vec3 point2;
    float min_distance = -1.0f;
    float max_distance = -1.0f;
    spring_settings_t spring_settings = {};
    constexpr bool operator==(const vierkant::constraint::distance_t &other) const = default;
};

using constraint_t = std::variant<constraint::none_t, constraint::point_t, constraint::distance_t>;
}// namespace constraint

struct physics_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    enum Mode : uint32_t
    {
        INACTIVE = 0,
        ACTIVE,
        UPDATE,
        REMOVE
    } mode = INACTIVE;

    vierkant::BodyId body_id = {};
    collision::shape_t shape = collision::none_t{};
    std::optional<vierkant::transform_t> shape_transform = {};
    float mass = 0.f;
    float friction = 0.2f;
    float restitution = 0.f;
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    bool kinematic = false;
    bool sensor = false;
    constexpr bool operator==(const vierkant::physics_component_t &) const = default;
};

//! constraints can be attached to arbitrary objects and reference via vierkant::BodyId
struct constraint_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();
    struct body_constraint_t
    {
        constraint::constraint_t constraint = constraint::none_t{};
        vierkant::BodyId body_id1 = vierkant::BodyId::nil();
        vierkant::BodyId body_id2 = vierkant::BodyId::nil();
    };
    std::vector<body_constraint_t> body_constraints;
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

    struct debug_draw_result_t
    {
        GeometryConstPtr lines;
        const std::vector<vierkant::AABB> &aabbs;
        const std::vector<glm::vec4> &colors;
        const std::vector<std::pair<vierkant::transform_t, GeometryConstPtr>> &triangle_meshes;
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

    [[nodiscard]] debug_draw_result_t debug_render() const;

    void set_gravity(const glm::vec3 &g);
    [[nodiscard]] glm::vec3 gravity() const;

    bool add_object(uint32_t objectId, const vierkant::transform_t &transform,
                    const vierkant::physics_component_t &cmp);
    void remove_object(uint32_t objectId, const vierkant::physics_component_t &cmp = {});
    [[nodiscard]] bool contains(uint32_t objectId) const;

    bool add_constraints(const vierkant::constraint_component_t &constraint_cmp);
    void remove_constraint(const vierkant::ConstraintId &constraint_id);

    void set_callbacks(uint32_t objectId, const callbacks_t &callbacks);

    void set_threadpool(crocore::ThreadPool &pool);

    BodyInterface &body_interface();

    CollisionShapeId create_collision_shape(const collision::mesh_t &mesh_cmp, const glm::vec3 &scale = glm::vec3(1));

    CollisionShapeId create_convex_collision_shape(const collision::mesh_t &mesh_cmp,
                                                   const glm::vec3 &scale = glm::vec3(1));

    vierkant::CollisionShapeId create_collision_shape(const collision::shape_t &shape);

    vierkant::ConstraintId create_constraint(const constraint::constraint_t &constraint, uint32_t objectId1,
                                             uint32_t objectId2);

    collision::mesh_provider_fn mesh_provider;

private:
    struct engine;
    std::unique_ptr<engine, std::function<void(engine *)>> m_engine;
};


class PhysicsScene : public vierkant::Scene
{
public:
    ~PhysicsScene() override = default;

    static std::shared_ptr<PhysicsScene> create(const std::shared_ptr<vierkant::ObjectStore> &object_store = {});

    void add_object(const Object3DPtr &object) override;

    void remove_object(const Object3DPtr &object) override;

    void clear() override;

    void update(double time_delta) override;

    vierkant::PhysicsContext &physics_context() { return m_context; };
    [[nodiscard]] const vierkant::PhysicsContext &physics_context() const { return m_context; };

private:
    explicit PhysicsScene(const std::shared_ptr<vierkant::ObjectStore> &object_store);

    crocore::ThreadPool m_thread_pool{std::thread::hardware_concurrency() - 1};
    vierkant::PhysicsContext m_context{&m_thread_pool};
};

}//namespace vierkant