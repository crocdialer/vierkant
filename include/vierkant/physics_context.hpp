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
    LocalToBodyCOM = 0,
    World = 1,
};

enum class SpringMode : uint8_t
{
    FrequencyAndDamping = 0,
    StiffnessAndDamping = 1,
};

enum class MotorState
{
    Off = 0,
    Velocity = 1,
    Position = 2
};

enum class SwingType
{
    Cone = 0,
    Pyramid = 1
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

struct motor_t
{
    //! settings for the spring that is used to drive to the position target (not used when motor is a velocity motor).
    spring_settings_t spring_settings = {};

    //! minimum force to apply in case of a linear constraint (N). Usually this is -mMaxForceLimit unless you want a motor that can e.g. push but not pull. Not used when motor is an angular motor.
    float min_force_limit = -std::numeric_limits<float>::infinity();

    //! maximum force to apply in case of a linear constraint (N). Not used when motor is an angular motor.
    float max_force_limit = std::numeric_limits<float>::infinity();

    //! minimum torque to apply in case of a angular constraint (N m). Usually this is -mMaxTorqueLimit unless you want a motor that can e.g. push but not pull. Not used when motor is a position motor.
    float min_torque_limit = -std::numeric_limits<float>::infinity();
    float max_torque_limit = std::numeric_limits<float>::infinity();

    MotorState state = MotorState::Off;
    float target_velocity = 0.f;
    float target_position = 0.f;

    constexpr bool operator==(const vierkant::constraint::motor_t &other) const = default;
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
    glm::vec3 point1{0.f};
    glm::vec3 point2{0.f};
    float min_distance = -1.0f;
    float max_distance = -1.0f;
    spring_settings_t spring_settings = {};
    constexpr bool operator==(const vierkant::constraint::distance_t &other) const = default;
};

struct slider_t
{
    /// This determines in which space the constraint is setup, all properties below should be in the specified space
    ConstraintSpace space = ConstraintSpace::World;

    /// When mSpace is WorldSpace mPoint1 and mPoint2 can be automatically calculated based on the positions of the bodies when the constraint is created (the current relative position/orientation is chosen as the '0' position). Set this to false if you want to supply the attachment points yourself.
    bool auto_detect_point = false;

    /// Body 1 constraint reference frame (space determined by mSpace).
    /// Slider axis is the axis along which movement is possible (direction), normal axis is a perpendicular vector to define the frame.
    glm::vec3 point1{0.f};
    glm::vec3 slider_axis1 = glm::vec3(1.f, 0.f, 0.f);

    /// Body 2 constraint reference frame (space determined by mSpace)
    glm::vec3 point2{0.f};
    glm::vec3 slider_axis2 = glm::vec3(1.f, 0.f, 0.f);

    /// When the bodies move so that mPoint1 coincides with mPoint2 the slider position is defined to be 0, movement will be limited between [mLimitsMin, mLimitsMax] where mLimitsMin e [-inf, 0] and mLimitsMax e [0, inf]
    float limits_min = -std::numeric_limits<float>::infinity();
    float limits_max = std::numeric_limits<float>::infinity();

    /// When enabled, this makes the limits soft. When the constraint exceeds the limits, a spring force will pull it back.
    spring_settings_t limits_spring_settings;

    /// Maximum amount of friction force to apply (N) when not driven by a motor.
    float max_friction_force = 0.0f;

    /// In case the constraint is powered, this determines the motor settings around the sliding axis
    motor_t motor = {};

    constexpr bool operator==(const vierkant::constraint::slider_t &other) const = default;
};

struct hinge_t
{
    /// This determines in which space the constraint is setup, all properties below should be in the specified space
    ConstraintSpace space = ConstraintSpace::World;

    /// Body 1 constraint reference frame (space determined by mSpace).
    /// Hinge axis is the axis where rotation is allowed.
    /// When the normal axis of both bodies align in world space, the hinge angle is defined to be 0.
    /// mHingeAxis1 and mNormalAxis1 should be perpendicular. mHingeAxis2 and mNormalAxis2 should also be perpendicular.
    /// If you configure the joint in world space and create both bodies with a relative rotation you want to be defined as zero,
    /// you can simply set mHingeAxis1 = mHingeAxis2 and mNormalAxis1 = mNormalAxis2.
    glm::vec3 point1{0.f};
    glm::vec3 hinge_axis1 = glm::vec3(0.f, 1.f, 0.f);

    /// Body 2 constraint reference frame (space determined by mSpace)
    glm::vec3 point2{0.f};
    glm::vec3 hinge_axis2 = glm::vec3(0.f, 1.f, 0.f);

    /// Rotation around the hinge axis will be limited between [mLimitsMin, mLimitsMax] where mLimitsMin e [-pi, 0] and mLimitsMax e [0, pi].
    /// Both angles are in radians.
    float limits_min = -glm::pi<float>();
    float limits_max = glm::pi<float>();

    /// When enabled, this makes the limits soft. When the constraint exceeds the limits, a spring force will pull it back.
    spring_settings_t limits_spring_settings;

    /// Maximum amount of torque (N m) to apply as friction when the constraint is not powered by a motor
    float max_friction_torque = 0.0f;

    /// In case the constraint is powered, this determines the motor settings around the hinge axis
    motor_t motor = {};

    constexpr bool operator==(const vierkant::constraint::hinge_t &other) const = default;
};

struct gear_t
{
    ConstraintSpace space = ConstraintSpace::World;
    glm::vec3 hinge_axis1 = glm::vec3(1.f, 0.f, 0.f);
    glm::vec3 hinge_axis2 = glm::vec3(1.f, 0.f, 0.f);

    //! ratio between gears
    float ratio = 1.f;

    constexpr bool operator==(const vierkant::constraint::gear_t &other) const = default;
};

struct swing_twist_t
{
    ConstraintSpace space = ConstraintSpace::World;

    glm::vec3 position1{0.f};
    glm::vec3 twist_axis1 = glm::vec3(1.f, 0.f, 0.f);
    glm::vec3 plane_axis1 = glm::vec3(0.f, 1.f, 0.f);

    glm::vec3 position2{0.f};
    glm::vec3 twist_axis2 = glm::vec3(1.f, 0.f, 0.f);
    glm::vec3 plane_axis2 = glm::vec3(0.f, 1.f, 0.f);

    /// type of swing constraint that we want to use.
    constraint::SwingType swing_type = constraint::SwingType::Cone;

    //! swing rotation limits
    float normal_half_cone_angle = 0.0f;
    float plane_half_cone_angle = 0.0f;

    //! twist rotation limits
    float twist_min_angle = 0.0f;
    float twist_max_angle = 0.0f;

    //! maximum torque (N m) to apply as friction when constraint is not powered by a motor
    float max_friction_torque = 0.0f;

    constraint::motor_t swing_motor;
    constraint::motor_t twist_motor;

    constexpr bool operator==(const vierkant::constraint::swing_twist_t &other) const = default;
};

using constraint_t = std::variant<constraint::none_t, constraint::point_t, constraint::distance_t, constraint::slider_t,
                                  constraint::hinge_t, constraint::gear_t, constraint::swing_twist_t>;
}// namespace constraint

struct physics_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();

    enum Mode : uint32_t
    {
        INACTIVE = 0,
        ACTIVE,
        CONSTRAINT_UPDATE,
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

    bool add_constraints(uint32_t objectId, const vierkant::constraint_component_t &constraint_cmp);
    void remove_constraints(uint32_t objectId);

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