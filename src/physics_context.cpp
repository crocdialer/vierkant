//#define BT_USE_DOUBLE_PRECISION
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>

#include <LinearMath/btConvexHull.h>
#include <LinearMath/btConvexHullComputer.h>
#include <LinearMath/btGeometryUtil.h>
#include <LinearMath/btPolarDecomposition.h>

#include <btBulletDynamicsCommon.h>

#include <unordered_set>
#include <vierkant/physics_context.hpp>

// Define a dummy function that uses a symbol from bar1
extern "C" [[maybe_unused]] void DummyLinkHelper()
{
    auto verts = btAlignedObjectArray<btVector3>();
    auto eq = btAlignedObjectArray<btVector3>();
    btGeometryUtil::getPlaneEquationsFromVertices(verts, eq);
    btGeometryUtil::getVerticesFromPlaneEquations(eq, verts);

    btConvexHullComputer p2;
    p2.compute((float *) nullptr, 0, 0, 0.f, 0.f);

    (void)CProfileSample("");
    (void)btDiscreteDynamicsWorld(nullptr, nullptr, nullptr, nullptr);
    (void)btMultiBody(0, 0, {}, false, false);
    (void)btPolarDecomposition();
    HullLibrary hl;
    HullResult hr;
    hl.CreateConvexHull({}, hr);
}

namespace vierkant
{

typedef std::shared_ptr<btCollisionShape> btCollisionShapePtr;
typedef std::shared_ptr<btRigidBody> btRigidBodyPtr;
typedef std::shared_ptr<btSoftBody> btSoftBodyPtr;
typedef std::shared_ptr<btTypedConstraint> btTypedConstraintPtr;
typedef std::shared_ptr<btSoftRigidDynamicsWorld> btSoftRigidDynamicsWorldPtr;
typedef std::shared_ptr<btDynamicsWorld> btDynamicsWorldPtr;

inline btVector3 type_cast(const glm::vec3 &the_vec) { return {the_vec[0], the_vec[1], the_vec[2]}; }

inline btTransform type_cast(const glm::mat4 &the_transform)
{
    btTransform ret;
    ret.setFromOpenGLMatrix(&the_transform[0][0]);
    return ret;
}

inline const glm::vec3 &type_cast(const btVector3 &the_vec) { return reinterpret_cast<const glm::vec3 &>(the_vec); }

inline glm::mat4 to_mat4(const btTransform &the_transform)
{
    glm::mat4 m;
    the_transform.getOpenGLMatrix(glm::value_ptr(m));
    return m;
}

inline const btQuaternion &type_cast(const glm::quat &q)
{
    auto tmp = (void *) &q;
    return *((const btQuaternion *) tmp);
}

inline const glm::quat &type_cast(const btQuaternion &q)
{
    auto tmp = (void *) &q;
    return *((const glm::quat *) tmp);
}

inline vierkant::transform_t to_transform(const btTransform &t)
{
    vierkant::transform_t ret;
    btQuaternion q;
    t.getBasis().getRotation(q);
    memcpy(glm::value_ptr(ret.rotation), &q[0], sizeof(ret.rotation));
    ret.translation = type_cast(t.getOrigin());
    return ret;
}

struct MotionState : public btMotionState
{
    vierkant::Object3DPtr m_object;

    explicit MotionState(vierkant::Object3DPtr obj) : m_object(std::move(obj)) {}
    ~MotionState() override = default;

    //! synchronizes world transform from user to physics
    void getWorldTransform(btTransform &centerOfMassWorldTrans) const override
    {
        auto t = m_object->global_transform();
        centerOfMassWorldTrans.setRotation(type_cast(t.rotation));
        centerOfMassWorldTrans.setOrigin(type_cast(t.translation));
    }

    //! synchronizes world transform from physics to user
    void setWorldTransform(const btTransform &centerOfMassWorldTrans) override
    {
        m_object->transform.rotation = type_cast(centerOfMassWorldTrans.getRotation());
        m_object->transform.translation = type_cast(centerOfMassWorldTrans.getOrigin());
    }
};

class BulletDebugDrawer : public btIDebugDraw
{
public:
    std::unordered_map<glm::vec4, std::vector<glm::vec3>> line_map;

    inline void clear()
    {
        for(auto &[col, lines]: line_map) lines.clear();
    }

    inline void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) override
    {
        glm::vec4 c(color[0], color[1], color[2], 1.f);
        auto &lines = line_map[c];
        lines.insert(lines.end(), {type_cast(from), type_cast(to)});
        lines.insert(lines.end(), {c, c});
    }

    void drawContactPoint(const btVector3 & /*PointOnB*/, const btVector3 & /*normalOnB*/, btScalar /*distance*/,
                          int /*lifeTime*/, const btVector3 & /*color*/) override
    {}

    void draw3dText(const btVector3 & /*location*/, const char * /*textString*/) override{};

    void reportErrorWarning(const char *warningString) override { spdlog::warn(warningString); }

    void setDebugMode(int /*debugMode*/) override {}
    [[nodiscard]] int getDebugMode() const override { return DBG_DrawWireframe; }
};

class MeshInterface : public btStridingMeshInterface
{
public:
    explicit MeshInterface(const vierkant::mesh_buffer_bundle_t &mesh_bundle)
        : entries(mesh_bundle.entries), vertex_stride(mesh_bundle.vertex_stride),
          vertex_buffer(mesh_bundle.vertex_buffer), index_buffer(mesh_bundle.index_buffer)
    {}

    /// get read and write access to a subpart of a triangle mesh
    /// this subpart has a continuous array of vertices and indices
    /// in this way the mesh can be handled as chunks of memory with striding
    /// very similar to OpenGL vertexarray support
    /// make a call to unLockVertexBase when the read and write access is finished
    void getLockedVertexIndexBase(unsigned char **vertexbase, int &numverts, PHY_ScalarType &type, int &stride,
                                  unsigned char **indexbase, int &indexstride, int &numfaces,
                                  PHY_ScalarType &indicestype, int subpart) override
    {
        getLockedReadOnlyVertexIndexBase((const uint8_t **) vertexbase, numverts, type, stride,
                                         (const uint8_t **) indexbase, indexstride, numfaces, indicestype, subpart);
    }

    void getLockedReadOnlyVertexIndexBase(const unsigned char **vertexbase, int &numverts, PHY_ScalarType &type,
                                          int &stride, const unsigned char **indexbase, int &indexstride, int &numfaces,
                                          PHY_ScalarType &indicestype, int subpart) const override
    {
        const auto &entry = entries[subpart];
        const auto &lod0 = entry.lods.front();
        *vertexbase = vertex_buffer.data() + vertex_stride * entry.vertex_offset;
        numverts = static_cast<int>(entry.num_vertices);
        type = PHY_FLOAT;
        stride = static_cast<int>(vertex_stride);
        *indexbase = reinterpret_cast<const unsigned char *>(index_buffer.data() + lod0.base_index);
        indexstride = 3 * sizeof(vierkant::index_t);
        numfaces = static_cast<int>(lod0.num_indices / 3);
        indicestype = PHY_INTEGER;
    }

    /// unLockVertexBase finishes the access to a subpart of the triangle mesh
    /// make a call to unLockVertexBase when the read and write access (using getLockedVertexIndexBase) is finished
    void unLockVertexBase(int /*subpart*/) override {}

    void unLockReadOnlyVertexBase(int /*subpart*/) const override{};

    /// getNumSubParts returns the number of seperate subparts
    /// each subpart has a continuous array of vertices and indices
    [[nodiscard]] int getNumSubParts() const override { return static_cast<int>(entries.size()); }

    void preallocateVertices(int /*numverts*/) override{};
    void preallocateIndices(int /*numindices*/) override{};

private:
    //! entries for sub-meshes/buffers
    std::vector<Mesh::entry_t> entries;

    //! vertex-stride in bytes
    uint32_t vertex_stride = 0;

    //! combined array of vertices (vertex-footprint varies hence encoded as raw-bytes)
    std::vector<uint8_t> vertex_buffer;

    //! combined array of indices
    std::vector<index_t> index_buffer;
};

class TriangleMeshShape : public btBvhTriangleMeshShape
{
public:
    TriangleMeshShape(std::unique_ptr<MeshInterface> meshInterface, bool useQuantizedAabbCompression,
                      bool buildBvh = true)
        : btBvhTriangleMeshShape(meshInterface.get(), useQuantizedAabbCompression, buildBvh),
          m_striding_mesh(std::move(meshInterface))
    {}

    ///optionally pass in a larger bvh aabb, used for quantization. This allows for deformations within this aabb
    TriangleMeshShape(std::unique_ptr<MeshInterface> meshInterface, bool useQuantizedAabbCompression,
                      const btVector3 &bvhAabbMin, const btVector3 &bvhAabbMax, bool buildBvh = true)
        : btBvhTriangleMeshShape(meshInterface.get(), useQuantizedAabbCompression, bvhAabbMin, bvhAabbMax, buildBvh),
          m_striding_mesh(std::move(meshInterface))
    {}

private:
    std::unique_ptr<MeshInterface> m_striding_mesh;
};

class BulletContext
{
public:
    struct rigid_body_item_t
    {
        RigidBodyId id;
        std::unique_ptr<MotionState> motion_state;
        btRigidBodyPtr rigid_body;
    };

    struct object_item_t
    {
        uint32_t id = 0;
        physics_component_t::callbacks_t callbacks;
    };

    BulletContext()
    {
        world->setGravity(btVector3(0, -9.87, 0));

        // debug drawer
        debug_drawer = std::make_shared<BulletDebugDrawer>();
        world->setDebugDrawer(debug_drawer.get());

        // tick callback
        world->setInternalTickCallback(&_tick_callback, static_cast<void *>(this));
    }

    static inline void _tick_callback(btDynamicsWorld *world, btScalar timestep)
    {
        auto *ctx = static_cast<BulletContext *>(world->getWorldUserInfo());
        ctx->tick_callback(timestep);
    }

    void tick_callback(btScalar /*timestep*/)
    {
        auto last_collision_pairs = std::move(collision_pairs);
        int numManifolds = world->getDispatcher()->getNumManifolds();

        for(int i = 0; i < numManifolds; i++)
        {
            auto contactManifold = world->getDispatcher()->getManifoldByIndexInternal(i);
            auto itemA = object_items.at(contactManifold->getBody0());
            auto itemB = object_items.at(contactManifold->getBody1());

            auto key = std::make_pair(contactManifold->getBody0(), contactManifold->getBody1());

            int numContacts = contactManifold->getNumContacts();
            for(int j = 0; j < numContacts; j++)
            {
                btManifoldPoint &pt = contactManifold->getContactPoint(j);
                if(pt.getDistance() < 0.f)
                {
                    collision_pairs.insert(key);

                    if(!last_collision_pairs.contains(key))
                    {
                        // contact added
                        if(itemA.callbacks.contact_begin) { itemA.callbacks.contact_begin(itemB.id); }
                        if(itemB.callbacks.contact_begin) { itemB.callbacks.contact_begin(itemA.id); }
                    }
                    if(itemA.callbacks.collision) { itemA.callbacks.collision(itemB.id); }
                    if(itemB.callbacks.collision) { itemB.callbacks.collision(itemA.id); }
                    last_collision_pairs.erase(key);
                    break;
                    //                                const btVector3& ptA = pt.getPositionWorldOnA();
                    //                                const btVector3& ptB = pt.getPositionWorldOnB();
                    //                                const btVector3& normalOnB = pt.m_normalWorldOnB;
                }
            }
        }

        // leftover pairs indicate a contact ended
        for(const auto &[objA, objB]: last_collision_pairs)
        {
            // wake sleeping islands after potential removal of an object
            objA->activate();
            objB->activate();

            auto itemA = object_items.at(objA);
            auto itemB = object_items.at(objB);

            if(itemA.callbacks.contact_end) { itemA.callbacks.contact_end(itemB.id); }
            if(itemB.callbacks.contact_end) { itemB.callbacks.contact_end(itemA.id); }
        }
    }

    std::shared_ptr<btDefaultCollisionConfiguration> configuration =
            std::make_shared<btDefaultCollisionConfiguration>();

    std::shared_ptr<btCollisionDispatcher> dispatcher = std::make_shared<btCollisionDispatcher>(configuration.get());
    std::shared_ptr<btBroadphaseInterface> broadphase = std::make_shared<btDbvtBroadphase>();

    ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
    std::shared_ptr<btConstraintSolver> solver = std::make_shared<btSequentialImpulseConstraintSolver>();

    btSoftRigidDynamicsWorldPtr world = std::make_shared<btSoftRigidDynamicsWorld>(dispatcher.get(), broadphase.get(),
                                                                                   solver.get(), configuration.get());
    std::shared_ptr<BulletDebugDrawer> debug_drawer;

    std::unordered_map<CollisionShapeId, btCollisionShapePtr> collision_shapes;

    //! maps object-id -> rigid-body
    std::unordered_map<uint32_t, rigid_body_item_t> rigid_bodies;
    std::unordered_map<const btCollisionObject *, object_item_t> object_items;

    std::unordered_map<ConstraintId, btTypedConstraintPtr> constraints;

    // current collision-pairs
    std::unordered_set<std::pair<const btCollisionObject *, const btCollisionObject *>,
                       vierkant::pair_hash<const btCollisionObject *, const btCollisionObject *>>
            collision_pairs;
};

struct PhysicsContext::engine
{
    BulletContext bullet;
};

PhysicsContext::PhysicsContext() : m_engine(std::make_unique<PhysicsContext::engine>()) {}

PhysicsContext::PhysicsContext(PhysicsContext &&other) noexcept { std::swap(m_engine, other.m_engine); }

PhysicsContext &PhysicsContext::operator=(PhysicsContext other)
{
    std::swap(m_engine, other.m_engine);
    return *this;
}

void PhysicsContext::step_simulation(float timestep, int max_sub_steps, float fixed_time_step)
{
    if(m_engine->bullet.world) { m_engine->bullet.world->stepSimulation(timestep, max_sub_steps, fixed_time_step); }
}

CollisionShapeId PhysicsContext::create_collision_shape(const mesh_buffer_bundle_t &mesh_bundle, const glm::vec3 &scale)
{
    auto mesh_shape = std::make_shared<TriangleMeshShape>(std::make_unique<MeshInterface>(mesh_bundle), true, true);
    mesh_shape->setLocalScaling(type_cast(scale));
    vierkant::CollisionShapeId new_id;
    m_engine->bullet.collision_shapes[new_id] = std::move(mesh_shape);
    return new_id;
}

CollisionShapeId PhysicsContext::create_convex_collision_shape(const mesh_buffer_bundle_t &mesh_bundle,
                                                               const glm::vec3 &scale)
{
    const auto &entry = mesh_bundle.entries.front();
    auto verts = (btScalar *) (mesh_bundle.vertex_buffer.data() + entry.vertex_offset * mesh_bundle.vertex_stride);
    auto hull_shape =
            std::make_shared<btConvexHullShape>(verts, (int) entry.num_vertices, (int) mesh_bundle.vertex_stride);
    hull_shape->setLocalScaling(type_cast(scale));
    vierkant::CollisionShapeId new_id;
    m_engine->bullet.collision_shapes[new_id] = std::move(hull_shape);
    return new_id;
}

CollisionShapeId PhysicsContext::create_box_shape(const glm::vec3 &half_extents)
{
    auto box_shape = std::make_shared<btBoxShape>(type_cast(half_extents));
    vierkant::CollisionShapeId new_id;
    m_engine->bullet.collision_shapes[new_id] = std::move(box_shape);
    return new_id;
}

CollisionShapeId PhysicsContext::create_plane_shape(const Plane &plane)
{
    auto plane_shape = std::make_shared<btStaticPlaneShape>(type_cast(plane.normal()), plane.coefficients.w);
    vierkant::CollisionShapeId new_id;
    m_engine->bullet.collision_shapes[new_id] = std::move(plane_shape);
    return new_id;
}

CollisionShapeId PhysicsContext::create_capsule_shape(float radius, float height)
{
    auto capsule_shape = std::make_shared<btCapsuleShape>(radius, height);
    vierkant::CollisionShapeId new_id;
    m_engine->bullet.collision_shapes[new_id] = std::move(capsule_shape);
    return new_id;
}

CollisionShapeId PhysicsContext::create_cylinder_shape(const glm::vec3 &half_extents)
{
    auto cylinder_shape = std::make_shared<btCylinderShape>(type_cast(half_extents));
    vierkant::CollisionShapeId new_id;
    m_engine->bullet.collision_shapes[new_id] = std::move(cylinder_shape);
    return new_id;
}

RigidBodyId PhysicsContext::add_object(const Object3DPtr &obj)
{
    if(obj->has_component<physics_component_t>())
    {
        const vierkant::object_component auto &cmp = obj->get_component<physics_component_t>();
        auto it = m_engine->bullet.rigid_bodies.find(obj->id());

        // was already there
        if(it != m_engine->bullet.rigid_bodies.end()) { return it->second.id; }

        // shape-lookup
        auto shape_it = m_engine->bullet.collision_shapes.find(cmp.shape_id);

        if(shape_it != m_engine->bullet.collision_shapes.end())
        {
            const auto col_shape = shape_it->second.get();
            btVector3 local_inertia;

            // required per object!?
            if(cmp.mass != 0.f) { col_shape->calculateLocalInertia(cmp.mass, local_inertia); }
            col_shape->setLocalScaling(type_cast(obj->transform.scale));

            // create new rigid-body
            auto &rigid_item = m_engine->bullet.rigid_bodies[obj->id()];
            rigid_item.motion_state = std::make_unique<MotionState>(obj);
            rigid_item.rigid_body =
                    std::make_shared<btRigidBody>(cmp.mass, rigid_item.motion_state.get(), col_shape, local_inertia);
            rigid_item.rigid_body->setFriction(cmp.friction);
            rigid_item.rigid_body->setRollingFriction(cmp.rolling_friction);
            rigid_item.rigid_body->setSpinningFriction(cmp.spinning_friction);
            rigid_item.rigid_body->setRestitution(cmp.restitution);

            if(cmp.kinematic)
            {
                rigid_item.rigid_body->setCollisionFlags(rigid_item.rigid_body->getCollisionFlags() |
                                                         btCollisionObject::CF_KINEMATIC_OBJECT);
                rigid_item.rigid_body->setActivationState(DISABLE_DEACTIVATION);
            }

            // add to world
            if(cmp.collision_only)
            {
                rigid_item.rigid_body->setCollisionFlags(rigid_item.rigid_body->getCollisionFlags() |
                                                         btCollisionObject::CF_STATIC_OBJECT);
                m_engine->bullet.world->addCollisionObject(
                        rigid_item.rigid_body.get(), btBroadphaseProxy::SensorTrigger,
                        int(btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter));
            }
            else { m_engine->bullet.world->addRigidBody(rigid_item.rigid_body.get()); }

            auto &object_item = m_engine->bullet.object_items[rigid_item.rigid_body.get()];
            object_item.id = obj->id();
            object_item.callbacks = cmp.callbacks;
            return rigid_item.id;
        }
        else { spdlog::warn("could not find collision-shape"); }
    }
    return vierkant::RigidBodyId::nil();
}

void PhysicsContext::remove_object(const Object3DPtr &obj)
{
    auto it = m_engine->bullet.rigid_bodies.find(obj->id());

    // found
    if(it != m_engine->bullet.rigid_bodies.end())
    {
        auto body = it->second.rigid_body.get();
        m_engine->bullet.world->removeRigidBody(body);
        m_engine->bullet.rigid_bodies.erase(it);
    }
}

const std::unordered_map<glm::vec4, std::vector<glm::vec3>> &PhysicsContext::debug_render()
{
    m_engine->bullet.debug_drawer->clear();
    m_engine->bullet.world->debugDrawWorld();
    return m_engine->bullet.debug_drawer->line_map;
}

void PhysicsContext::set_gravity(const glm::vec3 &g) { m_engine->bullet.world->setGravity(type_cast(g)); }

glm::vec3 PhysicsContext::gravity() const { return type_cast(m_engine->bullet.world->getGravity()); }

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
    m_context.step_simulation(static_cast<float>(time_delta));
}

std::shared_ptr<PhysicsScene> PhysicsScene::create() { return std::shared_ptr<PhysicsScene>(new PhysicsScene()); }

}//namespace vierkant
