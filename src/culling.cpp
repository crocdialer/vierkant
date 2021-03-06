//
// Created by crocdialer on 6/14/20.
//

#include "vierkant/Visitor.hpp"
#include "vierkant/culling.hpp"

namespace vierkant
{

struct scoped_stack_push
{
    std::stack<glm::mat4> &stack;

    scoped_stack_push(std::stack<glm::mat4> &stack, const glm::mat4 &mat) : stack(stack){ stack.push(mat); }

    ~scoped_stack_push(){ stack.pop(); }
};

class CullVisitor : public vierkant::Visitor
{
public:

    CullVisitor(vierkant::CameraPtr cam, bool check_intersection) :
            m_frustum(cam->frustum()),
            m_camera(std::move(cam)),
            m_check_intersection(check_intersection)
    {
        m_transform_stack.push(m_camera->view_matrix());
    };

    void visit(vierkant::Object3D &object) override
    {
        if(should_visit(object))
        {
            scoped_stack_push scoped_stack_push(m_transform_stack, m_transform_stack.top() * object.transform());
            for(Object3DPtr &child : object.children()){ child->accept(*this); }
        }
    }

    void visit(vierkant::MeshNode &node) override
    {
        if(should_visit(node))
        {
            // create drawables
            auto mesh_drawables = vierkant::Renderer::create_drawables(node.mesh,
                                                                       m_transform_stack.top() * node.transform());

            for(auto &drawable : mesh_drawables)
            {
                drawable.matrices.projection = m_camera->projection_matrix();
            }

            // move drawables into cull_result
            std::move(mesh_drawables.begin(), mesh_drawables.end(), std::back_inserter(m_cull_result.drawables));

            // continue scenegraph-traversal
            scoped_stack_push scoped_stack_push(m_transform_stack, m_transform_stack.top() * node.transform());
            visit(static_cast<vierkant::Object3D &>(node));
        }
    }

    bool should_visit(vierkant::Object3D &object) override
    {
        if(object.enabled() && check_tags(m_tags, object.tags()))
        {
            if(m_check_intersection)
            {
                // check intersection of aabb in eye-coords with view-frustum
                auto aabb = object.aabb().transform(m_transform_stack.top() * object.transform());
                return m_frustum.intersect(aabb);
            }
            return true;
        }
        return false;
    }

    std::set<std::string> m_tags;

    vierkant::Frustum m_frustum;

    vierkant::CameraPtr m_camera;

    bool m_check_intersection;

    std::stack<glm::mat4> m_transform_stack;

    cull_result_t m_cull_result;
};

cull_result_t cull(const SceneConstPtr &scene, const CameraPtr &cam, bool check_intersection,
                   const std::set<std::string> &tags)
{
    CullVisitor cull_visitor(cam, false);
    scene->root()->accept(cull_visitor);
    cull_visitor.m_cull_result.scene = scene;
    cull_visitor.m_cull_result.camera = cam;
    return std::move(cull_visitor.m_cull_result);
}

}