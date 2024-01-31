#pragma once

//#define BT_USE_DOUBLE_PRECISION
#include <btBulletDynamicsCommon.h>

#include <vierkant/Mesh.hpp>
#include <vierkant/Rasterizer.hpp>

class btThreadSupportInterface;

namespace vierkant::physics
{

typedef std::shared_ptr<btCollisionShape> btCollisionShapePtr;
typedef std::shared_ptr<const btCollisionShape> btCollisionShapeConstPtr;
typedef std::shared_ptr<btCollisionObject> btCollisionObjectPtr;
typedef std::shared_ptr<const btCollisionObject> btCollisionConstObjectPtr;
typedef std::shared_ptr<btDynamicsWorld> btDynamicsWorldPtr;
typedef std::shared_ptr<const btDynamicsWorld> btDynamicsWorldConstPtr;
typedef std::shared_ptr<btIDebugDraw> btIDebugDrawPtr;

btVector3 type_cast(const glm::vec3 &the_vec);
btTransform type_cast(const glm::mat4 &the_transform);
glm::vec3 type_cast(const btVector3 &the_vec);
glm::mat4 type_cast(const btTransform &the_transform);
btCollisionShapePtr create_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                           const glm::vec3 &the_scale = glm::vec3(1));
btCollisionShapePtr create_convex_collision_shape(const vierkant::mesh_buffer_bundle_t &mesh_bundle,
                                                  const glm::vec3 &the_scale = glm::vec3(1));

class BulletDebugDrawer : public btIDebugDraw
{
public:
    BulletDebugDrawer(){
            //            m_mesh_lines = gl::Mesh::create();
            //            m_mesh_lines->geometry()->set_primitive_type(GL_LINES);
            //
            //            m_mesh_points = gl::Mesh::create();
            //            m_mesh_points->material()->set_shader(gl::create_shader(gl::ShaderType::POINTS_COLOR));
            //            m_mesh_points->material()->set_point_size(5.f);
            //            m_mesh_points->material()->set_point_attenuation(1.f, 0.f, 0.05f);
            //            m_mesh_points->geometry()->set_primitive_type(GL_POINTS);

            //            setDebugMode(DBG_DrawWireframe /*| DBG_DrawContactPoints*/);
    };

    inline void drawLine(const btVector3 & /*from*/, const btVector3 & /*to*/, const btVector3 & /*color*/) override
    {
        //            m_mesh_lines->geometry()->append_vertex(glm::vec3(from.x(), from.y(), from.z()));
        //            m_mesh_lines->geometry()->append_vertex(glm::vec3(to.x(), to.y(), to.z()));
        //            m_mesh_lines->geometry()->append_color(glm::vec4(color.x(), color.y(), color.z(), 1.0f));
        //            m_mesh_lines->geometry()->append_color(glm::vec4(color.x(), color.y(), color.z(), 1.0f));
    }

    void drawContactPoint(const btVector3 & /*PointOnB*/, const btVector3 & /*normalOnB*/, btScalar /*distance*/,
                          int /*lifeTime*/, const btVector3 & /*color*/) override
    {
        //            m_mesh_points->geometry()->append_vertex(glm::vec3(PointOnB.x(), PointOnB.y(), PointOnB.z()));
        //            m_mesh_points->geometry()->append_color(glm::vec4(color.x(), color.y(), color.z(), 1.0f));
    }

    void reportErrorWarning(const char *warningString) override { spdlog::warn(warningString); }

    void setDebugMode(int /*debugMode*/) override {}
    [[nodiscard]] int getDebugMode() const override { return DBG_DrawWireframe; }

private:
};

class physics_context
{
public:
    //        explicit physics_context(int num_tasks = 1):m_maxNumTasks(num_tasks){};
    physics_context();
    ~physics_context();

    //        void init();
    void step_simulation(float timestep, int max_sub_steps = 1, float fixed_time_step = 1.f / 60.f);
    void debug_render(vierkant::Rasterizer &renderer, const vierkant::transform_t &transform,
                      const glm::mat4 &projection);
    void teardown();

    const btDynamicsWorldConstPtr dynamicsWorld() const { return m_dynamicsWorld; };
    const btDynamicsWorldPtr &dynamicsWorld() { return m_dynamicsWorld; };

    const std::vector<btRigidBody *> &bounding_bodies() const { return m_bounding_bodies; };
    std::vector<btRigidBody *> &bounding_bodies() { return m_bounding_bodies; };

    const std::set<btCollisionShapePtr> &collisionShapes() const { return m_collisionShapes; };
    std::set<btCollisionShapePtr> &collisionShapes() { return m_collisionShapes; };

    void near_callback(btBroadphasePair &collisionPair, btCollisionDispatcher &dispatcher,
                       btDispatcherInfo &dispatchInfo);

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
    void set_world_boundaries(const glm::vec3 &the_half_extents, const glm::vec3 &the_origin = glm::vec3(0));

    void attach_constraints(float the_thresh);

    /*!
         * internal tick callback, do not call directly
         */
    void tick_callback(btScalar timeStep);

private:
    //        std::unordered_map<gl::MeshPtr, btCollisionShapePtr> m_mesh_shape_map{};
    //        std::unordered_map<gl::MeshPtr, btRigidBody*> m_mesh_rigidbody_map{};

    std::set<btCollisionShapePtr> m_collisionShapes;
    std::shared_ptr<btBroadphaseInterface> m_broadphase;
    std::shared_ptr<btCollisionDispatcher> m_dispatcher;
    std::shared_ptr<btConstraintSolver> m_solver;
    std::shared_ptr<btDefaultCollisionConfiguration> m_collisionConfiguration;
    btDynamicsWorldPtr m_dynamicsWorld;

    //        uint32_t m_maxNumTasks;
    //        std::shared_ptr<btThreadSupportInterface> m_threadSupportCollision;
    //        std::shared_ptr<btThreadSupportInterface> m_threadSupportSolver;

    std::shared_ptr<BulletDebugDrawer> m_debug_drawer;

    std::vector<btCollisionShapePtr> m_bounding_shapes;
    std::vector<btRigidBody *> m_bounding_bodies;
};

}//namespace vierkant::physics
