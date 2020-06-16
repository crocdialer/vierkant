//
// Created by crocdialer on 6/14/20.
//

#include "vierkant/Visitor.hpp"
#include "vierkant/culling.hpp"

namespace vierkant
{

class CullVisitor : public vierkant::Visitor
{
public:

    CullVisitor(vierkant::CameraPtr cam) : vierkant::Visitor(true), m_camera(std::move(cam))
    {
        m_transform_stack.push(m_camera->view_matrix());
    };

    void visit(vierkant::Object3D &object) override
    {
        if(should_visit(object))
        {
            m_transform_stack.push(m_transform_stack.top() * object.transform());
            for(Object3DPtr &child : object.children()){ child->accept(*this); }
            m_transform_stack.pop();
        }
    }

    void visit(vierkant::Mesh &mesh) override
    {
        // create drawables
        auto mesh_ptr = std::dynamic_pointer_cast<vierkant::Mesh>(mesh.shared_from_this());
        auto mesh_drawables = vierkant::Renderer::create_drawables(mesh_ptr);

        for(auto &drawable : mesh_drawables)
        {
            drawable.matrices.modelview = m_transform_stack.top() * drawable.matrices.modelview;
            drawable.matrices.projection = m_camera->projection_matrix();
        }

        // move drawables into cull_result
        std::move(mesh_drawables.begin(), mesh_drawables.end(), std::back_inserter(m_cull_result.drawables));
        visit(static_cast<vierkant::Object3D&>(mesh));
    }

    bool should_visit(vierkant::Object3D &object) override
    {
        if(object.enabled() && check_tags(m_tags, object.tags()))
        {
            // TODO: check intersection with view-frustum
            bool is_visible = true;

            return is_visible;
        }
        return false;
    }

    std::set<std::string> m_tags;

    vierkant::CameraPtr m_camera;

    std::stack<glm::mat4> m_transform_stack;

    cull_result_t m_cull_result;
};

cull_result_t cull(const SceneConstPtr &scene, const CameraPtr &cam, const std::set<std::string> &tags)
{
    CullVisitor cull_visitor(cam);
    scene->root()->accept(cull_visitor);
    return std::move(cull_visitor.m_cull_result);
}

}