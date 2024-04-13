#include <unordered_set>
#include <vierkant/physics_context.hpp>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

// STL includes
#include <cstdarg>
#include <iostream>

// Callback for traces, connect this to your own trace function if you have one
static void trace_impl(const char *inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    spdlog::debug(buffer);
}

namespace vierkant
{

bool operator==(const vierkant::physics_component_t &lhs, const vierkant::physics_component_t &rhs)
{
    if(&lhs == &rhs) { return true; }
    //    if(lhs.shape != rhs.shape) { return false; }
    if(lhs.mass != rhs.mass) { return false; }
    if(lhs.friction != rhs.friction) { return false; }
    if(lhs.restitution != rhs.restitution) { return false; }
    if(lhs.linear_damping != rhs.linear_damping) { return false; }
    if(lhs.angular_damping != rhs.angular_damping) { return false; }
    if(lhs.sensor != rhs.sensor) { return false; }
    if(lhs.kinematic != rhs.kinematic) { return false; }
    if(lhs.need_update != rhs.need_update) { return false; }
    //    if(lhs.callbacks.collision != rhs.callbacks.collision) { return false; }
    //    if(lhs.callbacks.contact_begin != rhs.callbacks.contact_begin) { return false; }
    //    if(lhs.callbacks.collision != rhs.callbacks.collision) { return false; }
    return true;
}

// Callback for asserts, connect this to your own assert handler if you have one
[[maybe_unused]] static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile,
                                              uint inLine)
{
    spdlog::error("{} : {} : ({}) {}", inFile, inLine, inExpression, inMessage ? inMessage : "");
    return true;
};

///// Implementation of a JobSystem using a thread pool
//class JobSystemThreadPool final : public JPH::JobSystemWithBarrier
//{
//public:
//
//    /// Creates a thread pool.
//    /// @see JobSystemThreadPool::Init
//    JobSystemThreadPool(uint inMaxJobs, uint inMaxBarriers, int inNumThreads = -1);
//    JobSystemThreadPool() = default;
//    ~JobSystemThreadPool() override;
//
//    /// Initialize the thread pool
//    /// @param inMaxJobs Max number of jobs that can be allocated at any time
//    /// @param inMaxBarriers Max number of barriers that can be allocated at any time
//    /// @param inNumThreads Number of threads to start (the number of concurrent jobs is 1 more because the main thread will also run jobs while waiting for a barrier to complete). Use -1 to auto detect the amount of CPU's.
//    void Init(uint inMaxJobs, uint inMaxBarriers, int inNumThreads = -1);
//
//    // See JobSystem
//    int GetMaxConcurrency() const override { return 0; }
//    JobHandle CreateJob(const char *inName, JPH::ColorArg inColor, const JobFunction &inJobFunction,
//                                uint32_t inNumDependencies = 0) override;
//
//    /// Change the max concurrency after initialization
//    void SetNumThreads(int inNumThreads)
//    {
//        StopThreads();
//        StartThreads(inNumThreads);
//    }
//
//protected:
//    // See JobSystem
//    void QueueJob(Job *inJob) override;
//    void QueueJobs(Job **inJobs, uint inNumJobs) override;
//    void FreeJob(Job *inJob) override;
//
//private:
//    /// Start/stop the worker threads
//    void StartThreads(int inNumThreads);
//    void StopThreads();
//
//    /// Entry point for a thread
//    void ThreadMain(int inThreadIndex);
//
//    /// Get the head of the thread that has processed the least amount of jobs
//    inline uint GetHead() const;
//
//    /// Internal helper function to queue a job
//    inline void QueueJobInternal(Job *inJob);
//};

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers
{
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};// namespace Layers

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch(inObject1)
        {
            case Layers::NON_MOVING: return inObject2 == Layers::MOVING;// Non moving only collides with moving
            case Layers::MOVING: return true;                           // Moving collides with everything
            default: JPH_ASSERT(false); return false;
        }
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr uint32_t NUM_LAYERS(2);
};// namespace BroadPhaseLayers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    [[nodiscard]] uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch((JPH::BroadPhaseLayer::Type) inLayer)
        {
            case(JPH::BroadPhaseLayer::Type) BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case(JPH::BroadPhaseLayer::Type) BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif// JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS]{};
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        switch(inLayer1)
        {
            case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING: return true;
            default: JPH_ASSERT(false); return false;
        }
    }
};

class BodyInterfaceImpl : public vierkant::PhysicsContext::BodyInterface
{
public:
    BodyInterfaceImpl(JPH::BodyInterface &jolt_body_interface,
                      const std::unordered_map<uint32_t, JPH::BodyID> &body_id_map)
        : m_jolt_body_interface(jolt_body_interface), m_body_id_map(body_id_map)
    {}

    virtual ~BodyInterfaceImpl() = default;

    [[nodiscard]] bool get_transform(uint32_t objectId, vierkant::transform_t &t) const override
    {
        auto it = m_body_id_map.find(objectId);
        if(it != m_body_id_map.end())
        {
            JPH::RVec3 position;
            JPH::Quat rotation{};
            m_jolt_body_interface.GetPositionAndRotation(it->second, position, rotation);
            t.translation = {position.GetX(), position.GetY(), position.GetZ()};
            t.rotation = {rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ()};
            return true;
        }
        return false;
    }

    void set_transform(uint32_t objectId, const vierkant::transform_t &t) const override
    {
        auto it = m_body_id_map.find(objectId);
        if(it != m_body_id_map.end())
        {
            auto position = JPH::RVec3(t.translation.x, t.translation.y, t.translation.z);
            auto rotation = JPH::Quat(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w);
            m_jolt_body_interface.SetPositionAndRotation(it->second, position, rotation, JPH::EActivation::Activate);
        }
    }

    void add_force(uint32_t objectId, const glm::vec3 &force, const glm::vec3 &offset) override
    {
        auto it = m_body_id_map.find(objectId);
        if(it != m_body_id_map.end())
        {
            m_jolt_body_interface.AddForce(it->second, JPH::RVec3(force.x, force.y, force.z),
                                           JPH::RVec3(offset.x, offset.y, offset.z));
        }
    }

    void add_impulse(uint32_t objectId, const glm::vec3 &impulse, const glm::vec3 &offset) override
    {
        auto it = m_body_id_map.find(objectId);
        if(it != m_body_id_map.end())
        {
            m_jolt_body_interface.AddImpulse(it->second, JPH::RVec3(impulse.x, impulse.y, impulse.z),
                                             JPH::RVec3(offset.x, offset.y, offset.z));
        }
    }

    [[nodiscard]] glm::vec3 velocity(uint32_t objectId) const override
    {
        auto it = m_body_id_map.find(objectId);
        if(it != m_body_id_map.end())
        {
            auto velocity = m_jolt_body_interface.GetLinearVelocity(it->second);
            return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
        }
        return {};
    }

    void set_velocity(uint32_t objectId, const glm::vec3 &velocity) override
    {
        auto it = m_body_id_map.find(objectId);
        if(it != m_body_id_map.end())
        {
            m_jolt_body_interface.SetLinearVelocity(it->second, JPH::Vec3(velocity.x, velocity.y, velocity.z));
        }
    }

private:
    //! lookup of body-ids
    JPH::BodyInterface &m_jolt_body_interface;
    const std::unordered_map<uint32_t, JPH::BodyID> &m_body_id_map;
};

class JoltContext : public JPH::BodyActivationListener, public JPH::ContactListener
{
public:
    JoltContext()
    {
        // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
        // This needs to be done before any other Jolt function is called.
        JPH::RegisterDefaultAllocator();

        // register trace-function
        JPH::Trace = &trace_impl;

        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

        // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
        // It is not directly used in this example but still required.
        m_factory = std::make_unique<JPH::Factory>();
        JPH::Factory::sInstance = m_factory.get();

        // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
        // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
        // If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
        JPH::RegisterTypes();

        m_temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        m_job_system = std::make_unique<JPH::JobSystemThreadPool>(
                cMaxPhysicsJobs, cMaxPhysicsBarriers, static_cast<int>(std::thread::hardware_concurrency() - 1));
        physics_system.Init(m_maxBodies, m_numBodyMutexes, m_maxBodyPairs, m_maxContactConstraints,
                            m_broad_phase_layer_interface, m_object_vs_broadphase_layer_filter,
                            m_object_vs_object_layer_filter);

        // A body activation listener gets notified when bodies activate and go to sleep
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        physics_system.SetBodyActivationListener(this);

        // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        physics_system.SetContactListener(this);

        body_system = std::make_unique<BodyInterfaceImpl>(physics_system.GetBodyInterface(), body_id_map);
    }

    // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps
    // in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
    inline void update(float delta, int num_steps = 1)
    {
        physics_system.Update(delta, num_steps, m_temp_allocator.get(), m_job_system.get());
    }

    void OnBodyActivated(const JPH::BodyID & /*inBodyID*/, uint64_t /*inBodyUserData*/) override
    {
        spdlog::debug("A body got activated");
    }

    void OnBodyDeactivated(const JPH::BodyID & /*inBodyID*/, uint64_t /*inBodyUserData*/) override
    {
        spdlog::debug("A body went to sleep");
    }

    JPH::ValidateResult OnContactValidate(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                                          JPH::RVec3Arg /*inBaseOffset*/,
                                          const JPH::CollideShapeResult & /*inCollisionResult*/) override
    {
        spdlog::debug("Contact validate callback");

        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                        const JPH::ContactManifold & /*inManifold*/, JPH::ContactSettings & /*ioSettings*/) override
    {
        spdlog::debug("A contact was added");
    }

    void OnContactPersisted(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                            const JPH::ContactManifold & /*inManifold*/, JPH::ContactSettings & /*ioSettings*/) override
    {
        spdlog::debug("A contact was persisted");
    }

    void OnContactRemoved(const JPH::SubShapeIDPair & /*inSubShapePair*/) override
    {
        spdlog::debug("A contact was removed");
    }

    ~JoltContext() override
    {
        // Unregisters all types with the factory and cleans up the default material
        JPH::UnregisterTypes();
        JPH::Factory::sInstance = nullptr;
    }

    //! collision-shape storage
    std::unordered_map<vierkant::CollisionShapeId, JPH::Shape *> shapes;

    //! lookup of body-ids
    std::unordered_map<uint32_t, JPH::BodyID> body_id_map;

    //! the actual physics system.
    JPH::PhysicsSystem physics_system;

    std::unique_ptr<BodyInterfaceImpl> body_system;

private:
    /// Maximum amount of jobs to allow
    constexpr static uint32_t cMaxPhysicsJobs = 2048;

    /// Maximum amount of barriers to allow
    constexpr static uint32_t cMaxPhysicsBarriers = 8;

    // This is the max amount of rigid bodies that you can add to the physics system.
    // If you try to add more you'll get an error.
    uint32_t m_maxBodies = 65536;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access.
    // Set it to 0 for the default settings.
    uint32_t m_numBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
    // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase).
    // If you make this buffer too small the queue will fill up
    // and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
    uint32_t m_maxBodyPairs = 65536;

    // This is the maximum size of the contact constraint buffer.
    // If more contacts (collisions between bodies) are detected than this number then these contacts will be ignored
    // and bodies will start interpenetrating / fall through the world.
    uint32_t m_maxContactConstraints = 10240;

    // Create mapping table from object layer to broadphase layer
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    BPLayerInterfaceImpl m_broad_phase_layer_interface;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    ObjectVsBroadPhaseLayerFilterImpl m_object_vs_broadphase_layer_filter;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    ObjectLayerPairFilterImpl m_object_vs_object_layer_filter;

    // We need a temp allocator for temporary allocations during the physics update. We're
    // pre-allocating 10 MB to avoid having to do allocations during the physics update.
    // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
    // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
    // malloc / free.
    std::unique_ptr<JPH::TempAllocatorImpl> m_temp_allocator;

    // We need a job system that will execute physics jobs on multiple threads. Typically
    // you would implement the JobSystem interface yourself and let Jolt Physics run on top
    // of your own job scheduler. JobSystemThreadPool is an example implementation.
    std::unique_ptr<JPH::JobSystemThreadPool> m_job_system;

    // Create a factory, this class is responsible for creating instances of classes
    // based on their name or hash and is mainly used for deserialization of saved data.
    // It is not directly used in this example but still required.
    std::unique_ptr<JPH::Factory> m_factory;
};

struct PhysicsContext::engine
{
    JoltContext jolt;
};

PhysicsContext::PhysicsContext() : m_engine(std::make_unique<PhysicsContext::engine>()) {}

PhysicsContext::PhysicsContext(PhysicsContext &&other) noexcept { std::swap(m_engine, other.m_engine); }

PhysicsContext &PhysicsContext::operator=(PhysicsContext other)
{
    std::swap(m_engine, other.m_engine);
    return *this;
}

void PhysicsContext::step_simulation(float timestep, int max_sub_steps)
{
    m_engine->jolt.update(timestep, max_sub_steps);
}

CollisionShapeId PhysicsContext::create_collision_shape(const mesh_buffer_bundle_t & /*mesh_bundle*/,
                                                        const glm::vec3 & /*scale*/)
{
    return CollisionShapeId::nil();
}

CollisionShapeId PhysicsContext::create_convex_collision_shape(const mesh_buffer_bundle_t &mesh_bundle,
                                                               const glm::vec3 &scale)
{
    //    JPH::ConvexHullShape()

    for(const auto &entry: mesh_bundle.entries)
    {
        JPH::Array<JPH::Vec3> points(entry.num_vertices);
        auto data = mesh_bundle.vertex_buffer.data() + entry.vertex_offset;
        for(uint32_t i = 0; i < entry.num_vertices; ++i, data += mesh_bundle.vertex_stride)
        {
            auto pos = *reinterpret_cast<const glm::vec3 *>(data) * scale;
            points[i] = JPH::Vec3(pos.x, pos.y, pos.z);
        }
        JPH::ConvexHullShapeSettings hull_shape_settings(points);
        JPH::Shape::ShapeResult shape_result;

        auto shape = new JPH::ConvexHullShape(hull_shape_settings, shape_result);

        auto mp = shape->GetMassProperties();
        (void) mp;
        if(shape_result.IsValid())
        {
            vierkant::CollisionShapeId new_id;
            m_engine->jolt.shapes[new_id] = shape;
            return new_id;
        }
        delete shape;
    }
    return CollisionShapeId::nil();
}

void PhysicsContext::add_object(const Object3DPtr &obj)
{
    if(obj->has_component<physics_component_t>())
    {
        const auto &t = obj->transform;
        const vierkant::object_component auto &cmp = obj->get_component<physics_component_t>();
        auto shape_id = create_collision_shape(cmp.shape);

        if(shape_id)
        {
            auto &body_interface = m_engine->jolt.physics_system.GetBodyInterface();

            bool has_mass = cmp.mass > 0.f;
            auto layer = has_mass ? Layers::MOVING : Layers::NON_MOVING;
            auto motion_type = has_mass ? JPH::EMotionType::Dynamic
                                        : (cmp.kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static);
            auto body_create_info = JPH::BodyCreationSettings(
                    m_engine->jolt.shapes[shape_id], JPH::RVec3(t.translation.x, t.translation.y, t.translation.z),
                    JPH::Quat(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w), motion_type, layer);
            body_create_info.mIsSensor = cmp.sensor;

            body_create_info.mFriction = cmp.friction;
            body_create_info.mRestitution = cmp.restitution;
            body_create_info.mLinearDamping = cmp.linear_damping;
            body_create_info.mAngularDamping = cmp.angular_damping;
            body_create_info.mMotionQuality = JPH::EMotionQuality::LinearCast;
            //            if(dynamic_cast<const JPH::ConvexHullShape *>(body_create_info.GetShape()))
            //            {
            //                auto half_extent = obj->aabb().half_extents();
            //                body_create_info.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            //                body_create_info.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(
            //                        2.0f * JPH::Vec3(half_extent.x, half_extent.y, half_extent.z), cmp.mass);
            //            }
            JPH::BodyID jolt_bodyId = body_interface.CreateAndAddBody(body_create_info, JPH::EActivation::Activate);

            body_interface.SetUserData(jolt_bodyId, obj->id());
            m_engine->jolt.body_id_map[obj->id()] = jolt_bodyId;
        }
    }
}

void PhysicsContext::remove_object(const Object3DPtr &obj)
{
    auto it = m_engine->jolt.body_id_map.find(obj->id());
    if(it != m_engine->jolt.body_id_map.end())
    {
        auto &body_interface = m_engine->jolt.physics_system.GetBodyInterface();
        body_interface.RemoveBody(it->second);
        body_interface.DestroyBody(it->second);
        m_engine->jolt.body_id_map.erase(it);
    }
}

bool PhysicsContext::contains(const vierkant::Object3DPtr &obj) const
{
    return m_engine->jolt.body_id_map.contains(obj->id());
}

vierkant::PhysicsContext::BodyInterface &PhysicsContext::body_interface() { return *m_engine->jolt.body_system; }

GeometryConstPtr PhysicsContext::debug_render() { return nullptr; }

void PhysicsContext::set_gravity(const glm::vec3 &g)
{
    m_engine->jolt.physics_system.SetGravity(JPH::Vec3(g.x, g.y, g.z));
}

glm::vec3 PhysicsContext::gravity() const
{
    auto g = m_engine->jolt.physics_system.GetGravity();
    return {g.GetX(), g.GetY(), g.GetZ()};
}

CollisionShapeId PhysicsContext::create_collision_shape(const vierkant::collision::shape_t &shape)
{
    auto shape_id = std::visit(
            [this](auto &&s) -> CollisionShapeId {
                using T = std::decay_t<decltype(s)>;

                if constexpr(std::is_same_v<T, collision::box_t>)
                {
                    vierkant::CollisionShapeId new_id;
                    m_engine->jolt.shapes[new_id] =
                            new JPH::BoxShape(JPH::Vec3(s.half_extents.x, s.half_extents.y, s.half_extents.z));
                    return new_id;
                }
                if constexpr(std::is_same_v<T, collision::sphere_t>)
                {
                    vierkant::CollisionShapeId new_id;
                    m_engine->jolt.shapes[new_id] = new JPH::SphereShape(s.radius);
                    return new_id;
                }
                if constexpr(std::is_same_v<T, collision::cylinder_t>)
                {
                    vierkant::CollisionShapeId new_id;
                    m_engine->jolt.shapes[new_id] = new JPH::CylinderShape(s.height / 2.f, s.radius);
                    return new_id;
                }
                if constexpr(std::is_same_v<T, collision::capsule_t>)
                {
                    vierkant::CollisionShapeId new_id;
                    m_engine->jolt.shapes[new_id] = new JPH::CapsuleShape(s.height / 2.f, s.radius);
                    return new_id;
                }
                if constexpr(std::is_same_v<T, CollisionShapeId>) { return s; }
                return CollisionShapeId::nil();
            },
            shape);
    return shape_id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void PhysicsScene::add_object(const Object3DPtr &object)
{
    vierkant::Scene::add_object(object);
    m_context.add_object(object);
}

void PhysicsScene::remove_object(const Object3DPtr &object)
{
    m_context.remove_object(object);
    vierkant::Scene::remove_object(object);
}

void PhysicsScene::clear()
{
    m_context = {};
    Scene::clear();
}

void PhysicsScene::update(double time_delta)
{
    Scene::update(time_delta);
    auto view = registry()->view<physics_component_t>();
    for(const auto &[entity, cmp]: view.each())
    {
        if(cmp.need_update)
        {
            auto obj = object_by_id(static_cast<uint32_t>(entity))->shared_from_this();
            m_context.remove_object(obj);
            m_context.add_object(obj);
            cmp.need_update = false;
        }

        // update transform bruteforce
        auto obj = object_by_id(static_cast<uint32_t>(entity));
        m_context.body_interface().get_transform(static_cast<uint32_t>(entity), obj->transform);
    }
    m_context.step_simulation(static_cast<float>(time_delta), 4);
}

std::shared_ptr<PhysicsScene> PhysicsScene::create() { return std::shared_ptr<PhysicsScene>(new PhysicsScene()); }

}//namespace vierkant

size_t std::hash<vierkant::physics_component_t>::operator()(vierkant::physics_component_t const &c) const
{
    size_t h = 0;
    //    vierkant::hash_combine(h, c.shape_id);
    vierkant::hash_combine(h, c.mass);
    vierkant::hash_combine(h, c.friction);
    vierkant::hash_combine(h, c.restitution);
    vierkant::hash_combine(h, c.linear_damping);
    vierkant::hash_combine(h, c.angular_damping);
    vierkant::hash_combine(h, c.kinematic);
    vierkant::hash_combine(h, c.sensor);
    vierkant::hash_combine(h, c.need_update);
    return h;
}