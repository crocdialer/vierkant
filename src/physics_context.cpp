//#define BT_USE_DOUBLE_PRECISION
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <btBulletDynamicsCommon.h>

#include <vierkant/physics_context.hpp>

class btThreadSupportInterface;

namespace vierkant
{
typedef std::shared_ptr<btCollisionShape> btCollisionShapePtr;
typedef std::shared_ptr<btSoftRigidDynamicsWorld> btSoftRigidDynamicsWorldPtr;
typedef std::shared_ptr<btIDebugDraw> btIDebugDrawPtr;

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

inline vierkant::transform_t to_transform(const btTransform &t)
{
    vierkant::transform_t ret;
    btQuaternion q;
    t.getBasis().getRotation(q);
    memcpy(glm::value_ptr(ret.rotation), &q[0], sizeof(ret.rotation));
    ret.translation = type_cast(t.getOrigin());
    return ret;
}

class BulletDebugDrawer : public btIDebugDraw
{
public:
    BulletDebugDrawer() = default;

    inline void drawLine(const btVector3 & from, const btVector3 & to, const btVector3 & color) override
    {
        m_geometry->positions.insert(m_geometry->positions.end(), {type_cast(from), type_cast(to)});
        glm::vec4 c(color[0], color[1], color[2], 1.f);
        m_geometry->colors.insert(m_geometry->colors.end(), {c, c});
    }

    void drawContactPoint(const btVector3 & /*PointOnB*/, const btVector3 & /*normalOnB*/, btScalar /*distance*/,
                          int /*lifeTime*/, const btVector3 & /*color*/) override
    {

    }

    void draw3dText(const btVector3 & /*location*/, const char * /*textString*/) override{};

    void reportErrorWarning(const char *warningString) override { spdlog::warn(warningString); }

    void setDebugMode(int /*debugMode*/) override {}
    [[nodiscard]] int getDebugMode() const override { return DBG_DrawWireframe; }

private:
    vierkant::GeometryPtr m_geometry = vierkant::Geometry::create();
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
    //        explicit BulletContext(int num_tasks = 1):m_maxNumTasks(num_tasks){};
    BulletContext()
    {
        configuration = std::make_shared<btDefaultCollisionConfiguration>();

        ///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
        dispatcher = std::make_shared<btCollisionDispatcher>(configuration.get());
        broadphase = std::make_shared<btDbvtBroadphase>();

        ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
        solver = std::make_shared<btSequentialImpulseConstraintSolver>();
        world = std::make_shared<btSoftRigidDynamicsWorld>(dispatcher.get(), broadphase.get(), solver.get(),
                                                           configuration.get());

        world->setGravity(btVector3(0, -9.87, 0));

        // debug drawer
        m_debug_drawer = std::make_shared<BulletDebugDrawer>();
        world->setDebugDrawer(m_debug_drawer.get());

        // tick callback
        //        world->setInternalTickCallback(&_tick_callback, static_cast<void*>(this));
    }
    //    ~BulletContext();

    //    void near_callback(btBroadphasePair &collisionPair, btCollisionDispatcher &dispatcher,
    //                       btDispatcherInfo &dispatchInfo);

    //        /*!
    //         * Add a kinski::MeshPtr instance to the physics simulation,
    //         * with an optional collision shape.
    //         * If no collision shape is provided, a (static) btBvhTriangleMeshShape instance will be created.
    //         * return: A pointer to the newly created btRigidBody
    //         */
    //        btRigidBody* add_mesh_to_simulation(const gl::MeshPtr &the_mesh, float mass = 0.f,
    //                                            btCollisionShapePtr col_shape = btCollisionShapePtr());
    //
    //        bool remove_mesh_from_simulation(const gl::MeshPtr &the_mesh);
    //
    //        /*!
    //         * return a pointer to the corresponding btRigidBody for the_mesh
    //         * or nullptr if not found
    //         */
    //        btRigidBody* get_rigidbody_for_mesh(const gl::MeshPtr &the_mesh);

    /*!
         * set where to position static planes as boundaries for the entire physics scene
         */
    //    void set_world_boundaries(const glm::vec3 &the_half_extents, const glm::vec3 &the_origin = glm::vec3(0));
    //
    //    void attach_constraints(float the_thresh);
    //
    //    /*!
    //         * internal tick callback, do not call directly
    //         */
    //    void tick_callback(btScalar timeStep);

    //        std::unordered_map<gl::MeshPtr, btCollisionShapePtr> m_mesh_shape_map{};
    //        std::unordered_map<gl::MeshPtr, btRigidBody*> m_mesh_rigidbody_map{};

    std::unordered_map<CollisionShapeId, btCollisionShapePtr> collision_shapes;

    std::shared_ptr<btBroadphaseInterface> broadphase;
    std::shared_ptr<btCollisionDispatcher> dispatcher;
    std::shared_ptr<btConstraintSolver> solver;
    std::shared_ptr<btDefaultCollisionConfiguration> configuration;
    //    btDynamicsWorldPtr world;
    btSoftRigidDynamicsWorldPtr world;

    //        uint32_t m_maxNumTasks;
    //        std::shared_ptr<btThreadSupportInterface> m_threadSupportCollision;
    //        std::shared_ptr<btThreadSupportInterface> m_threadSupportSolver;

    std::shared_ptr<BulletDebugDrawer> m_debug_drawer;
    std::vector<btRigidBody *> m_bounding_bodies;
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
    btCollisionShapePtr hull_shape(
            new btConvexHullShape(verts, (int) entry.num_vertices, (int) mesh_bundle.vertex_stride));
    hull_shape->setLocalScaling(type_cast(scale));
    vierkant::CollisionShapeId new_id;
    m_engine->bullet.collision_shapes[new_id] = std::move(hull_shape);
    return new_id;
}

}//namespace vierkant
