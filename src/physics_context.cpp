#include <unordered_set>

#include <crocore/ThreadPool.hpp>
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
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/SoftBody/SoftBodyContactListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyShape.h>
#include <Jolt/RegisterTypes.h>

// STL includes
#include <Physics/Collision/Shape/ScaledShape.h>
#include <Physics/Collision/Shape/StaticCompoundShape.h>
#include <cstdarg>

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

inline glm::vec3 type_cast(const JPH::Vec3 &v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
inline JPH::Vec3 type_cast(const glm::vec3 &v) { return {v.x, v.y, v.z}; }
inline glm::vec4 type_cast(const JPH::Vec4 &v) { return {v.GetX(), v.GetY(), v.GetZ(), v.GetW()}; }
inline JPH::Vec4 type_cast(const glm::vec4 &v) { return {v.x, v.y, v.z, v.w}; }
inline glm::quat type_cast(const JPH::Quat &q) { return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()}; }
inline JPH::Quat type_cast(const glm::quat &q) { return {q.x, q.y, q.z, q.w}; }

inline vierkant::AABB type_cast(const JPH::AABox &in_aabb)
{
    return {type_cast(in_aabb.mMin), type_cast(in_aabb.mMax)};
}

inline vierkant::transform_t type_cast(const JPH::Mat44 &mat)
{
    glm::mat4 tmp(type_cast(mat.GetColumn4(0)), type_cast(mat.GetColumn4(1)), type_cast(mat.GetColumn4(2)),
                  type_cast(mat.GetColumn4(3)));
    vierkant::transform_t ret = vierkant::transform_cast(tmp);
    return ret;
}

// Callback for asserts, connect this to your own assert handler if you have one
[[maybe_unused]] static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile,
                                              uint32_t inLine)
{
    spdlog::error("{} : {} : ({}) {}", inFile, inLine, inExpression, inMessage ? inMessage : "");
    return true;
};

class JoltDebugRenderer final : public JPH::DebugRenderer
{
public:
    JoltDebugRenderer() { Initialize(); }

    /// Draw a single back face culled triangle
    void DrawTriangle(JPH::RVec3Arg /*inV1*/, JPH::RVec3Arg /*inV2*/, JPH::RVec3Arg /*inV3*/, JPH::ColorArg /*inColor*/,
                      ECastShadow /*inCastShadow*/) override
    {}

    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
    {
        line_geometry->positions.push_back(type_cast(inFrom));
        line_geometry->positions.push_back(type_cast(inTo));
        line_geometry->colors.push_back(type_cast(inColor.ToVec4()));
        line_geometry->colors.push_back(type_cast(inColor.ToVec4()));
    }

    JPH::DebugRenderer::Batch CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) override
    {
        return CreateTriangleBatch(inTriangles->mV, inTriangleCount * 3, nullptr, 0);
    };

    JPH::DebugRenderer::Batch CreateTriangleBatch(const Vertex *inVertices, int inVertexCount,
                                                  const uint32_t *inIndices, int inIndexCount) override
    {
        auto geom = vierkant::Geometry::create();
        geom->positions.resize(inVertexCount);
        geom->normals.resize(inVertexCount);
        geom->tex_coords.resize(inVertexCount);
        geom->colors.resize(inVertexCount, glm::vec4(1));
        for(int i = 0; i < inVertexCount; ++i)
        {
            const auto &v = inVertices[i];
            geom->positions[i] = {v.mPosition.x, v.mPosition.y, v.mPosition.z};
            geom->normals[i] = {v.mNormal.x, v.mNormal.y, v.mNormal.z};
            geom->tex_coords[i] = {v.mUV.x, v.mUV.y};
            geom->colors[i] = type_cast(v.mColor.ToVec4());
        }
        if(inIndices) { geom->indices = {inIndices, inIndices + inIndexCount}; }

        auto out_batch = new Batch;
        out_batch->triangles = geom;
        return out_batch;
    };

    void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox &inWorldSpaceBounds, float /*inLODScaleSq*/,
                      JPH::ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode /*inCullMode*/,
                      ECastShadow /*inCastShadow*/, EDrawMode inDrawMode) override
    {
        if(!inGeometry->mLODs.empty())
        {
            auto batch = (Batch *) (inGeometry->mLODs.front().mTriangleBatch.GetPtr());

            if(inDrawMode == JPH::DebugRenderer::EDrawMode::Solid)
            {
                aabbs.push_back(type_cast(inWorldSpaceBounds));
                colors.push_back(type_cast(inModelColor.ToVec4()));
                triangle_meshes.emplace_back(type_cast(inModelMatrix), batch->triangles);
            }
        }
    };

    void DrawText3D(JPH::RVec3Arg /*inPosition*/, const JPH::string_view & /*inString*/, JPH::ColorArg /*inColor*/,
                    float /*inHeight*/) override {};

    void clear()
    {
        line_geometry->positions.clear();
        line_geometry->colors.clear();
        aabbs.clear();
        colors.clear();
        triangle_meshes.clear();
    }

    GeometryPtr line_geometry = vierkant::Geometry::create();
    std::vector<vierkant::AABB> aabbs;
    std::vector<glm::vec4> colors;
    std::vector<std::pair<vierkant::transform_t, GeometryConstPtr>> triangle_meshes;

private:
    struct Batch : public JPH::RefTargetVirtual
    {
        // stores an indexed triangle geometry
        vierkant::GeometryPtr triangles;

        std::atomic<uint32_t> m_ref_count = 0;
        void AddRef() override { ++m_ref_count; }
        void Release() override
        {
            if(--m_ref_count == 0) delete this;
        }
    };
};

///// Implementation of a JoltJobSystem using a thread pool
class JoltJobSystem final : public JPH::JobSystemWithBarrier
{
public:
    JoltJobSystem(uint32_t max_jobs, uint32_t max_barriers, crocore::ThreadPool &pool)
        : JobSystemWithBarrier(max_barriers), m_threadpool(pool), m_jobs(max_jobs, max_jobs),
          m_max_concurrency(pool.num_threads() - 1)
    {}
    ~JoltJobSystem() override = default;

    [[nodiscard]] int GetMaxConcurrency() const override { return (int) m_max_concurrency; }
    JPH::JobHandle CreateJob(const char *name, JPH::ColorArg color, const JobFunction &inJobFunction,
                             uint32_t inNumDependencies) override
    {
        // loop until we can get a job from the free list
        uint32_t index;
        for(;;)
        {
            index = m_jobs.create(name, color, this, inJobFunction, inNumDependencies);
            if(index != crocore::fixed_size_free_list<Job>::s_invalid_index) break;
            JPH_ASSERT(false, "No jobs available!");
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        Job *job = &m_jobs.get(index);

        // Construct handle to keep a reference, the job is queued below and may immediately complete
        JobHandle handle(job);

        // If there are no dependencies, queue the job now
        if(inNumDependencies == 0) { queue(job); }

        return handle;
    }

    void SetMaxConcurrency(int num_tasks)
    {
        m_max_concurrency = std::min<size_t>(num_tasks, m_threadpool.num_threads());
    }

    // See JPH::JobSystem
    void QueueJob(Job *inJob) override { queue(inJob); }
    void QueueJobs(Job **inJobs, uint32_t inNumJobs) override
    {
        for(auto j = inJobs, end = inJobs + inNumJobs; j < end; ++j) { queue(*j); }
    }
    void FreeJob(Job *inJob) override { m_jobs.destroy(inJob); }

private:
    inline void queue(Job *inJob)
    {
        inJob->AddRef();
        m_threadpool.post_no_track([inJob] {
            inJob->Execute();
            inJob->Release();
        });
    }

    crocore::ThreadPool &m_threadpool;
    crocore::fixed_size_free_list<Job> m_jobs;
    std::atomic<size_t> m_max_concurrency;
};

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
class JoltObjectLayerPairFilter : public JPH::ObjectLayerPairFilter
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
class JoltBPLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
    JoltBPLayerInterface()
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
class JoltObjectVsBroadPhaseLayerFilter : public JPH::ObjectVsBroadPhaseLayerFilter
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
        if(auto body_id = get_body_id(objectId))
        {
            JPH::RVec3 position;
            JPH::Quat rotation{};
            m_jolt_body_interface.GetPositionAndRotation(*body_id, position, rotation);
            t.translation = type_cast(position);
            t.rotation = type_cast(rotation);
            return true;
        }
        return false;
    }

    void set_transform(uint32_t objectId, const vierkant::transform_t &t) const override
    {
        if(auto body_id = get_body_id(objectId))
        {
            auto position = type_cast(t.translation);
            auto rotation = type_cast(t.rotation);
            m_jolt_body_interface.SetPositionAndRotation(*body_id, position, rotation, JPH::EActivation::Activate);
        }
    }

    void add_force(uint32_t objectId, const glm::vec3 &force, const glm::vec3 &offset) override
    {
        if(auto body_id = get_body_id(objectId))
        {
            m_jolt_body_interface.AddForce(*body_id, type_cast(force), type_cast(offset));
        }
    }

    void add_impulse(uint32_t objectId, const glm::vec3 &impulse, const glm::vec3 &offset) override
    {
        if(auto body_id = get_body_id(objectId))
        {
            m_jolt_body_interface.AddImpulse(*body_id, type_cast(impulse), type_cast(offset));
        }
    }

    [[nodiscard]] glm::vec3 velocity(uint32_t objectId) const override
    {
        if(auto body_id = get_body_id(objectId))
        {
            auto velocity = m_jolt_body_interface.GetLinearVelocity(*body_id);
            return type_cast(velocity);
        }
        return {};
    }

    void set_velocity(uint32_t objectId, const glm::vec3 &velocity) override
    {
        if(auto body_id = get_body_id(objectId))
        {
            m_jolt_body_interface.SetLinearVelocity(*body_id, type_cast(velocity));
        }
    }

    void activate(uint32_t objectId) override
    {
        if(auto body_id = get_body_id(objectId)) { m_jolt_body_interface.ActivateBody(*body_id); }
    }

    void activate_in_aabb(const vierkant::AABB &aabb) override
    {
        m_jolt_body_interface.ActivateBodiesInAABox(JPH::AABox(type_cast(aabb.min), type_cast(aabb.max)),
                                                    m_broad_phase_layer_filter, m_object_layer_filter);
    }

    bool is_active(uint32_t objectId) override
    {
        if(auto body_id = get_body_id(objectId)) { return m_jolt_body_interface.IsActive(*body_id); }
        return false;
    }

private:
    [[nodiscard]] inline std::optional<JPH::BodyID> get_body_id(uint32_t objectId) const
    {
        auto it = m_body_id_map.find(objectId);
        if(it != m_body_id_map.end()) { return it->second; }
        return {};
    }

    //! lookup of body-ids
    JPH::BodyInterface &m_jolt_body_interface;
    JPH::ObjectLayerFilter m_object_layer_filter;
    JPH::BroadPhaseLayerFilter m_broad_phase_layer_filter;
    const std::unordered_map<uint32_t, JPH::BodyID> &m_body_id_map;
};

class JoltContext : public JPH::BodyActivationListener, public JPH::ContactListener, public JPH::SoftBodyContactListener
{
public:
    /// Maximum amount of jobs to allow
    constexpr static uint32_t max_physics_jobs = 2048;

    /// Maximum amount of barriers to allow
    constexpr static uint32_t max_physics_barriers = 8;

    explicit JoltContext(crocore::ThreadPool *const thread_pool = nullptr) : thread_pool(thread_pool)
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

        if(thread_pool)
        {
            job_system = std::make_unique<JoltJobSystem>(JoltContext::max_physics_jobs,
                                                         JoltContext::max_physics_barriers, *thread_pool);
        }
        else
        {
            job_system = std::make_unique<JPH::JobSystemThreadPool>(
                    max_physics_jobs, max_physics_barriers, static_cast<int>(std::thread::hardware_concurrency() - 1));
        }
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
        physics_system.SetSoftBodyContactListener(this);

        body_system = std::make_unique<BodyInterfaceImpl>(physics_system.GetBodyInterfaceNoLock(), body_id_map);

        JPH::DebugRenderer::sInstance = nullptr;
        debug_render = std::make_unique<JoltDebugRenderer>();
    }

    ~JoltContext() override
    {
        // stupid singleton-hack
        JPH::DebugRenderer::sInstance = debug_render.get();
        debug_render.reset();

        // Unregisters all types with the factory and cleans up the default material
        JPH::UnregisterTypes();
        JPH::Factory::sInstance = nullptr;
    }

    // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps
    // in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
    inline void update(float delta, int num_steps = 1)
    {
        physics_system.Update(delta, num_steps, m_temp_allocator.get(), job_system.get());
    }

    void OnBodyActivated(const JPH::BodyID & /*inBodyID*/, uint64_t /*inBodyUserData*/) override {}

    void OnBodyDeactivated(const JPH::BodyID & /*inBodyID*/, uint64_t /*inBodyUserData*/) override {}

    JPH::ValidateResult OnContactValidate(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                                          JPH::RVec3Arg /*inBaseOffset*/,
                                          const JPH::CollideShapeResult & /*inCollisionResult*/) override
    {
        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold & /*inManifold*/,
                        JPH::ContactSettings & /*ioSettings*/) override
    {
        std::shared_lock lock(mutex);

        auto cb_it = callback_map.find(inBody1.GetUserData());
        if(cb_it != callback_map.end())
        {
            if(cb_it->second.contact_begin)
            {
                cb_it->second.contact_begin(inBody1.GetUserData(), inBody2.GetUserData());
            }
        }

        cb_it = callback_map.find(inBody2.GetUserData());
        if(cb_it != callback_map.end())
        {
            if(cb_it->second.contact_begin)
            {
                cb_it->second.contact_begin(inBody2.GetUserData(), inBody1.GetUserData());
            }
        }
    }

    void OnContactPersisted(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                            const JPH::ContactManifold & /*inManifold*/, JPH::ContactSettings & /*ioSettings*/) override
    {
        //        spdlog::debug("A contact was persisted");
    }

    void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
    {
        std::shared_lock lock(mutex);
        const auto &bodyI = physics_system.GetBodyInterfaceNoLock();
        uint32_t obj1 = bodyI.GetUserData(inSubShapePair.GetBody1ID());
        uint32_t obj2 = bodyI.GetUserData(inSubShapePair.GetBody2ID());

        auto cb_it = callback_map.find(obj1);
        if(cb_it != callback_map.end())
        {
            if(cb_it->second.contact_end) { cb_it->second.contact_end(obj1, obj2); }
        }

        cb_it = callback_map.find(obj2);
        if(cb_it != callback_map.end())
        {
            if(cb_it->second.contact_end) { cb_it->second.contact_end(obj2, obj1); }
        }
    }

    JPH::SoftBodyValidateResult
    OnSoftBodyContactValidate([[maybe_unused]] const JPH::Body &inSoftBody,
                              [[maybe_unused]] const JPH::Body &inOtherBody,
                              [[maybe_unused]] JPH::SoftBodyContactSettings &ioSettings) override
    {
        return JPH::SoftBodyValidateResult::AcceptContact;
    }

    void OnSoftBodyContactAdded([[maybe_unused]] const JPH::Body &inSoftBody,
                                [[maybe_unused]] const JPH::SoftBodyManifold &inManifold) override
    {}

    //! collision-shape storage
    std::unordered_map<vierkant::CollisionShapeId, JPH::Ref<JPH::Shape>> shapes;

    //! lookup of body-ids
    std::unordered_map<uint32_t, JPH::BodyID> body_id_map;

    //! lookup of callback-structs
    std::unordered_map<uint32_t, vierkant::PhysicsContext::callbacks_t> callback_map;

    std::shared_mutex mutex;

    //! the actual physics system.
    JPH::PhysicsSystem physics_system;

    std::unique_ptr<BodyInterfaceImpl> body_system;

    //! job-system backing physics-jobs
    std::unique_ptr<JPH::JobSystemWithBarrier> job_system;
    crocore::ThreadPool *thread_pool = nullptr;

    std::unique_ptr<JoltDebugRenderer> debug_render;

private:
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
    JoltBPLayerInterface m_broad_phase_layer_interface;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    JoltObjectVsBroadPhaseLayerFilter m_object_vs_broadphase_layer_filter;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    JoltObjectLayerPairFilter m_object_vs_object_layer_filter;

    // We need a temp allocator for temporary allocations during the physics update. We're
    // pre-allocating 10 MB to avoid having to do allocations during the physics update.
    // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
    // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
    // malloc / free.
    std::unique_ptr<JPH::TempAllocatorImpl> m_temp_allocator;

    // Create a factory, this class is responsible for creating instances of classes
    // based on their name or hash and is mainly used for deserialization of saved data.
    // It is not directly used in this example but still required.
    std::unique_ptr<JPH::Factory> m_factory;
};

struct PhysicsContext::engine
{
    JoltContext jolt;
    explicit engine(crocore::ThreadPool *const thread_pool = nullptr) : jolt(thread_pool) {}
};

PhysicsContext::PhysicsContext(crocore::ThreadPool *const thread_pool)
    : m_engine(std::make_unique<PhysicsContext::engine>(thread_pool))
{}

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

CollisionShapeId PhysicsContext::create_collision_shape(const mesh_buffer_bundle_t &mesh_bundle, const glm::vec3 &scale)
{
    JPH::StaticCompoundShapeSettings compound_shape_settings;

    for(const auto &entry: mesh_bundle.entries)
    {
        const auto &lod0 = entry.lods.front();
        JPH::VertexList points(entry.num_vertices);
        uint32_t num_triangles = lod0.num_indices / 3;
        JPH::IndexedTriangleList triangles(num_triangles);

        auto data = mesh_bundle.vertex_buffer.data() + entry.vertex_offset;
        for(uint32_t i = 0; i < entry.num_vertices; ++i, data += mesh_bundle.vertex_stride)
        {
            auto p = *reinterpret_cast<const glm::vec3 *>(data) * scale;
            points[i] = {p.x, p.y, p.z};
        }
        for(uint32_t i = 0; i < num_triangles; i++)
        {
            uint32_t base_index = lod0.base_index + 3 * i;
            triangles[i] =
                    JPH::IndexedTriangle(mesh_bundle.index_buffer[base_index], mesh_bundle.index_buffer[base_index + 1],
                                         mesh_bundle.index_buffer[base_index + 2], 0);
        }
        JPH::MeshShapeSettings mesh_shape_settings(points, triangles);
        JPH::Shape::ShapeResult shape_result = mesh_shape_settings.Create();

        if(shape_result.IsValid())
        {
            auto scaled_shape = new JPH::ScaledShape(shape_result.Get(), type_cast(entry.transform.scale));
            compound_shape_settings.AddShape(type_cast(entry.transform.translation),
                                             type_cast(entry.transform.rotation), scaled_shape);
        }
    }

    JPH::Shape::ShapeResult shape_result = compound_shape_settings.Create();
    if(shape_result.IsValid())
    {
        vierkant::CollisionShapeId new_id;
        m_engine->jolt.shapes[new_id] = shape_result.Get();
        return new_id;
    }
    return CollisionShapeId::nil();
}

CollisionShapeId PhysicsContext::create_convex_collision_shape(const mesh_buffer_bundle_t &mesh_bundle,
                                                               const glm::vec3 &scale)
{
    JPH::StaticCompoundShapeSettings compound_shape_settings;

    for(const auto &entry: mesh_bundle.entries)
    {
        JPH::Array<JPH::Vec3> points(entry.num_vertices);
        auto data = mesh_bundle.vertex_buffer.data() + entry.vertex_offset;
        for(uint32_t i = 0; i < entry.num_vertices; ++i, data += mesh_bundle.vertex_stride)
        {
            auto v = *reinterpret_cast<const glm::vec3 *>(data) * scale;
            points[i] = {v.x, v.y, v.z};
        }
        JPH::ConvexHullShapeSettings hull_shape_settings(points);
        JPH::Shape::ShapeResult shape_result = hull_shape_settings.Create();

        if(shape_result.IsValid())
        {
            auto scaled_shape = new JPH::ScaledShape(shape_result.Get(), type_cast(entry.transform.scale));
            compound_shape_settings.AddShape(type_cast(entry.transform.translation),
                                             type_cast(entry.transform.rotation), scaled_shape);
        }
    }
    JPH::Shape::ShapeResult shape_result = compound_shape_settings.Create();
    if(shape_result.IsValid())
    {
        vierkant::CollisionShapeId new_id;
        m_engine->jolt.shapes[new_id] = shape_result.Get();
        return new_id;
    }
    return CollisionShapeId::nil();
}

void PhysicsContext::add_object(uint32_t objectId, const vierkant::transform_t &transform,
                                const vierkant::physics_component_t &cmp)
{
    auto shape_id = create_collision_shape(cmp.shape);

    if(shape_id)
    {
        const auto &shape = m_engine->jolt.shapes.at(shape_id);
        auto &body_interface = m_engine->jolt.physics_system.GetBodyInterface();

        float mass = cmp.mass;
        if(auto mesh_shape = std::get_if<collision::mesh_t>(&cmp.shape))
        {
            // no mass for static triangle-shapes, explodes otherwise
            if(!mesh_shape->convex_hull) { mass = 0.f; }
        }

        bool dynamic = mass > 0.f && !cmp.sensor;
        bool kinematic = mass > 0.f && !cmp.kinematic;
        auto layer = dynamic ? Layers::MOVING : Layers::NON_MOVING;
        auto motion_type = dynamic ? JPH::EMotionType::Dynamic
                                   : (kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static);

        auto scaled_shape = new JPH::ScaledShape(shape, type_cast(transform.scale));
        auto body_create_info = JPH::BodyCreationSettings(scaled_shape, type_cast(transform.translation),
                                                          type_cast(transform.rotation), motion_type, layer);

        auto mass_properties = body_create_info.GetMassProperties();
        mass_properties.ScaleToMass(mass);
        body_create_info.mMassPropertiesOverride = mass_properties;
        body_create_info.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;

        body_create_info.mIsSensor = cmp.sensor;
        body_create_info.mFriction = cmp.friction;
        body_create_info.mRestitution = cmp.restitution;
        body_create_info.mLinearDamping = cmp.linear_damping;
        body_create_info.mAngularDamping = cmp.angular_damping;
        body_create_info.mMotionQuality = JPH::EMotionQuality::LinearCast;

        {
            std::unique_lock lock(m_engine->jolt.mutex);
            JPH::BodyID jolt_bodyId = body_interface.CreateAndAddBody(body_create_info, JPH::EActivation::Activate);
            body_interface.SetUserData(jolt_bodyId, objectId);
            m_engine->jolt.body_id_map[objectId] = jolt_bodyId;
            spdlog::trace("PhysicsContext::add_object: obj: {} / body {}", objectId, jolt_bodyId.GetIndex());
        }
    }
}

void PhysicsContext::remove_object(uint32_t objectId)
{
    std::unique_lock lock(m_engine->jolt.mutex);
    auto it = m_engine->jolt.body_id_map.find(objectId);
    if(it != m_engine->jolt.body_id_map.end())
    {
        spdlog::trace("PhysicsContext::remove_object: obj: {} / body {}", objectId, it->second.GetIndex());
        auto &body_interface = m_engine->jolt.physics_system.GetBodyInterface();
        body_interface.RemoveBody(it->second);
        body_interface.DestroyBody(it->second);
        m_engine->jolt.body_id_map.erase(it);
        m_engine->jolt.callback_map.erase(objectId);
    }
}

bool PhysicsContext::contains(uint32_t objectId) const { return m_engine->jolt.body_id_map.contains(objectId); }

vierkant::PhysicsContext::BodyInterface &PhysicsContext::body_interface() { return *m_engine->jolt.body_system; }

PhysicsContext::debug_draw_result_t PhysicsContext::debug_render()
{
    m_engine->jolt.debug_render->clear();

    JPH::BodyManager::DrawSettings ds;
    ds.mDrawVelocity = true;
    m_engine->jolt.physics_system.DrawBodies(ds, m_engine->jolt.debug_render.get());
    return {m_engine->jolt.debug_render->line_geometry, m_engine->jolt.debug_render->aabbs,
            m_engine->jolt.debug_render->colors, m_engine->jolt.debug_render->triangle_meshes};
}

void PhysicsContext::set_gravity(const glm::vec3 &g) { m_engine->jolt.physics_system.SetGravity(type_cast(g)); }

glm::vec3 PhysicsContext::gravity() const { return type_cast(m_engine->jolt.physics_system.GetGravity()); }

CollisionShapeId PhysicsContext::create_collision_shape(const vierkant::collision::shape_t &shape)
{
    auto shape_id = std::visit(
            [this](auto &&s) -> CollisionShapeId {
                using T = std::decay_t<decltype(s)>;

                if constexpr(std::is_same_v<T, CollisionShapeId>)
                {
                    if(m_engine->jolt.shapes.contains(s)) { return s; }
                    return vierkant::CollisionShapeId::nil();
                    //                    assert(false);
                }

                vierkant::CollisionShapeId new_id;

                if constexpr(std::is_same_v<T, collision::box_t>)
                {
                    m_engine->jolt.shapes[new_id] = new JPH::BoxShape(type_cast(s.half_extents));
                }
                else if constexpr(std::is_same_v<T, collision::sphere_t>)
                {
                    m_engine->jolt.shapes[new_id] = new JPH::SphereShape(s.radius);
                }
                else if constexpr(std::is_same_v<T, collision::cylinder_t>)
                {
                    m_engine->jolt.shapes[new_id] = new JPH::CylinderShape(s.height / 2.f, s.radius);
                }
                else if constexpr(std::is_same_v<T, collision::capsule_t>)
                {
                    m_engine->jolt.shapes[new_id] = new JPH::CapsuleShape(s.height / 2.f, s.radius);
                }
                else if constexpr(std::is_same_v<T, collision::mesh_t>)
                {
                    if(mesh_provider)
                    {
                        auto assets = mesh_provider(s.mesh_id);
                        if(assets.bundle)
                        {
                            new_id = s.convex_hull ? create_convex_collision_shape(*assets.bundle)
                                                   : create_collision_shape(*assets.bundle);
                            return new_id;
                        }
                    }
                    return CollisionShapeId::nil();
                }
                return new_id;
            },
            shape);
    return shape_id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PhysicsContext::set_threadpool(crocore::ThreadPool &pool)
{
    m_engine->jolt.job_system =
            std::make_unique<JoltJobSystem>(JoltContext::max_physics_jobs, JoltContext::max_physics_barriers, pool);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PhysicsContext::set_callbacks(uint32_t objectId, const PhysicsContext::callbacks_t &callbacks)
{
    std::unique_lock lock(m_engine->jolt.mutex);
    m_engine->jolt.callback_map[objectId] = callbacks;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PhysicsScene::add_object(const Object3DPtr &object)
{
    vierkant::Scene::add_object(object);

    if(object)
    {
        auto phy_cmp_ptr = object->get_component_ptr<vierkant::physics_component_t>();
        if(phy_cmp_ptr) { m_context.add_object(object->id(), object->transform, *phy_cmp_ptr); }
    }
}

void PhysicsScene::remove_object(const Object3DPtr &object)
{
    if(object) { m_context.remove_object(object->id()); }
    vierkant::Scene::remove_object(object);
}

void PhysicsScene::clear()
{
    m_context = vierkant::PhysicsContext(&m_thread_pool);
    Scene::clear();
}

void PhysicsScene::update(double time_delta)
{
    Scene::update(time_delta);
    auto view = registry()->view<physics_component_t>();
    for(const auto &[entity, cmp]: view.each())
    {
        auto obj = object_by_id(static_cast<uint32_t>(entity));
        if(cmp.mode == physics_component_t::UPDATE)
        {
            if(auto mesh_shape = std::get_if<collision::mesh_t>(&cmp.shape))
            {
                if(auto mesh_cmp = obj->get_component_ptr<vierkant::mesh_component_t>())
                {
                    mesh_shape->mesh_id = mesh_cmp->mesh->id;
                }
            }
            m_context.remove_object(obj->id());
            m_context.add_object(obj->id(), obj->transform, cmp);
            cmp.mode = physics_component_t::ACTIVE;
        }
        else if(cmp.mode == physics_component_t::REMOVE)
        {
            m_context.remove_object(obj->id());
            obj->remove_component<physics_component_t>();
            continue;
        }

        if(cmp.kinematic)
        {
            // object -> physics
            m_context.body_interface().set_transform(static_cast<uint32_t>(entity), obj->transform);
        }
        else
        {
            // physics -> object
            m_context.body_interface().get_transform(static_cast<uint32_t>(entity), obj->transform);
        }
    }
    m_context.step_simulation(static_cast<float>(time_delta), 2);
}

std::shared_ptr<PhysicsScene> PhysicsScene::create() { return std::shared_ptr<PhysicsScene>(new PhysicsScene()); }

}//namespace vierkant

size_t std::hash<vierkant::physics_component_t>::operator()(vierkant::physics_component_t const &c) const
{
    size_t h = 0;
    vierkant::hash_combine(h, c.mode);
    vierkant::hash_combine(h, c.mass);
    vierkant::hash_combine(h, c.friction);
    vierkant::hash_combine(h, c.restitution);
    vierkant::hash_combine(h, c.linear_damping);
    vierkant::hash_combine(h, c.angular_damping);
    vierkant::hash_combine(h, c.kinematic);
    vierkant::hash_combine(h, c.sensor);
    return h;
}