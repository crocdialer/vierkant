#include <unordered_set>
#include <vierkant/physics_context.hpp>

//#define JPH_ENABLE_ASSERTS

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
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
    if(lhs.rolling_friction != rhs.rolling_friction) { return false; }
    if(lhs.spinning_friction != rhs.spinning_friction) { return false; }
    if(lhs.restitution != rhs.restitution) { return false; }
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
    spdlog::error("{} : {} : ({}) {}", inFile, inLine, inExpression, inMessage);
    return true;
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
static constexpr uint NUM_LAYERS(2);
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

// An example contact listener
class JoltContactListener : public JPH::ContactListener
{
public:
    // See: ContactListener
    JPH::ValidateResult OnContactValidate(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                                          JPH::RVec3Arg /*inBaseOffset*/,
                                          const JPH::CollideShapeResult & /*inCollisionResult*/) override
    {
        //        cout << "Contact validate callback" << endl;

        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                        const JPH::ContactManifold & /*inManifold*/, JPH::ContactSettings & /*ioSettings*/) override
    {
        //        cout << "A contact was added" << endl;
    }

    void OnContactPersisted(const JPH::Body & /*inBody1*/, const JPH::Body & /*inBody2*/,
                            const JPH::ContactManifold & /*inManifold*/, JPH::ContactSettings & /*ioSettings*/) override
    {
        //        cout << "A contact was persisted" << endl;
    }

    void OnContactRemoved(const JPH::SubShapeIDPair & /*inSubShapePair*/) override
    {
        //        cout << "A contact was removed" << endl;
    }
};

// An example activation listener
class JoltBodyActivationListener : public JPH::BodyActivationListener
{
public:
    void OnBodyActivated(const JPH::BodyID & /*inBodyID*/, uint64_t /*inBodyUserData*/) override
    {
        //        cout << "A body got activated" << endl;
    }

    void OnBodyDeactivated(const JPH::BodyID & /*inBodyID*/, uint64_t /*inBodyUserData*/) override
    {
        //        cout << "A body went to sleep" << endl;
    }
};

class JoltContext
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
        JPH::Factory::sInstance = new JPH::Factory();

        // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
        // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
        // If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
        JPH::RegisterTypes();

        m_physics_system.Init(m_maxBodies, m_numBodyMutexes, m_maxBodyPairs, m_maxContactConstraints,
                              m_broad_phase_layer_interface, m_object_vs_broadphase_layer_filter,
                              m_object_vs_object_layer_filter);

        m_physics_system.SetBodyActivationListener(&m_body_activation_listener);
        m_physics_system.SetContactListener(&m_contact_listener);
    }

    ~JoltContext()
    {
        // Unregisters all types with the factory and cleans up the default material
        JPH::UnregisterTypes();

        // Destroy the factory
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

private:
    // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    uint32_t m_maxBodies = 1024;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
    uint32_t m_numBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
    // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
    // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    uint32_t m_maxBodyPairs = 1024;

    // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
    // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
    uint32_t m_maxContactConstraints = 1024;

    // Create mapping table from object layer to broadphase layer
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    BPLayerInterfaceImpl m_broad_phase_layer_interface;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    ObjectVsBroadPhaseLayerFilterImpl m_object_vs_broadphase_layer_filter;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    ObjectLayerPairFilterImpl m_object_vs_object_layer_filter;

    // A body activation listener gets notified when bodies activate and go to sleep
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    JoltBodyActivationListener m_body_activation_listener;

    // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    JoltContactListener m_contact_listener;

    // the actual physics system.
    JPH::PhysicsSystem m_physics_system;
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

void PhysicsContext::step_simulation(float /*timestep*/, int /*max_sub_steps*/, float /*fixed_time_step*/) {}

CollisionShapeId PhysicsContext::create_collision_shape(const mesh_buffer_bundle_t & /*mesh_bundle*/,
                                                        const glm::vec3 & /*scale*/)
{
    return CollisionShapeId::nil();
}

CollisionShapeId PhysicsContext::create_convex_collision_shape(const mesh_buffer_bundle_t & /*mesh_bundle*/,
                                                               const glm::vec3 & /*scale*/)
{
    return CollisionShapeId::nil();
}

RigidBodyId PhysicsContext::add_object(const Object3DPtr &obj)
{
    if(obj->has_component<physics_component_t>())
    {
        //        const vierkant::object_component auto &cmp = obj->get_component<physics_component_t>();
    }
    return vierkant::RigidBodyId::nil();
}

void PhysicsContext::remove_object(const Object3DPtr & /*obj*/) {}

bool PhysicsContext::contains(const Object3DPtr & /*obj*/) const { return false; }

RigidBodyId PhysicsContext::body_id(const Object3DPtr & /*obj*/) const { return RigidBodyId ::nil(); }

GeometryConstPtr PhysicsContext::debug_render() { return nullptr; }

void PhysicsContext::set_gravity(const glm::vec3 & /*g*/) {}

glm::vec3 PhysicsContext::gravity() const { return {}; }

void PhysicsContext::apply_force(const vierkant::Object3DPtr & /*obj*/, const glm::vec3 & /*force*/,
                                 const glm::vec3 & /*offset*/)
{}

void PhysicsContext::apply_impulse(const Object3DPtr & /*obj*/, const glm::vec3 & /*impulse*/,
                                   const glm::vec3 & /*offset*/)
{}

glm::vec3 PhysicsContext::velocity(const Object3DPtr & /*obj*/) { return {}; }

void PhysicsContext::set_velocity(const Object3DPtr & /*obj*/, const glm::vec3 & /*velocity*/) {}

CollisionShapeId PhysicsContext::create_collision_shape(const vierkant::collision::shape_t &shape)
{
    auto shape_id = std::visit(
            [this](auto &&s) -> CollisionShapeId {
                using T = std::decay_t<decltype(s)>;

                //                if constexpr(std::is_same_v<T, collision::plane_t>)
                //                {
                //                    auto plane_shape = std::make_shared<btStaticPlaneShape>(type_cast(s.normal), s.d);
                //                    vierkant::CollisionShapeId new_id;
                //                    m_engine->bullet.collision_shapes[new_id] = std::move(plane_shape);
                //                    return new_id;
                //                }
                //                if constexpr(std::is_same_v<T, collision::box_t>)
                //                {
                //                }
                //                if constexpr(std::is_same_v<T, collision::sphere_t>)
                //                {
                //                }
                //                if constexpr(std::is_same_v<T, collision::cylinder_t>)
                //                {
                //                }
                //                if constexpr(std::is_same_v<T, collision::capsule_t>)
                //                {
                //                }
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
    }
    m_context.step_simulation(static_cast<float>(time_delta), 4, 1.f / 240.f);
}

std::shared_ptr<PhysicsScene> PhysicsScene::create() { return std::shared_ptr<PhysicsScene>(new PhysicsScene()); }

}//namespace vierkant

size_t std::hash<vierkant::physics_component_t>::operator()(vierkant::physics_component_t const &c) const
{
    size_t h = 0;
    //    vierkant::hash_combine(h, c.shape_id);
    vierkant::hash_combine(h, c.mass);
    vierkant::hash_combine(h, c.friction);
    vierkant::hash_combine(h, c.rolling_friction);
    vierkant::hash_combine(h, c.spinning_friction);
    vierkant::hash_combine(h, c.restitution);
    vierkant::hash_combine(h, c.kinematic);
    vierkant::hash_combine(h, c.sensor);
    vierkant::hash_combine(h, c.need_update);
    return h;
}