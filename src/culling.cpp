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

    CullVisitor(vierkant::CameraPtr cam,
                bool check_intersection,
                bool world_space) :
            m_frustum(cam->frustum()),
            m_camera(std::move(cam)),
            m_check_intersection(check_intersection)
    {
        if(!world_space){ m_transform_stack.push(m_camera->view_matrix()); }
        else{ m_transform_stack.push(glm::mat4(1)); }
    };

    void visit(vierkant::Object3D &object) override
    {
        if(should_visit(object))
        {
            scoped_stack_push scoped_stack_push(m_transform_stack, m_transform_stack.top() * object.transform());
            for(Object3DPtr &child: object.children()){ child->accept(*this); }
        }
    }

    void visit(vierkant::MeshNode &node) override
    {
        if(should_visit(node))
        {
            auto model_view = m_transform_stack.top() * node.transform();

//            auto entry_filter = [&model_view, &frustum = m_frustum](const Mesh::entry_t &entry) -> bool
//            {
//                if(!entry.enabled){ return false; }
//                auto aabb = entry.boundingbox.transform(model_view * entry.transform);
//                return vierkant::intersect(aabb, frustum);
//            };

            auto node_ptr = std::dynamic_pointer_cast<const vierkant::MeshNode>(node.shared_from_this());

            // keep track of meshes
            if(node.mesh)
            {
                m_cull_result.meshes.insert(node.mesh);

                if(node.has_component<animation_state_t>() &&
                   (node.mesh->root_bone || node.mesh->morph_buffer))
                {
                    m_cull_result.animated_nodes.insert(node_ptr);
                }
            }

            // create drawables
            vierkant::create_drawables_params_t drawable_params = {};
            drawable_params.mesh = node.mesh;
            drawable_params.model_view = model_view;

            if(node.has_component<animation_state_t>())
            {
                auto &animation_state = node.get_component<animation_state_t>();
                drawable_params.animation_index = animation_state.index;
                drawable_params.animation_time = animation_state.current_time;
            }
            auto mesh_drawables = vierkant::create_drawables(drawable_params);

            for(auto &drawable: mesh_drawables)
            {
                m_cull_result.node_map[drawable.id] = node_ptr;
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
                return vierkant::intersect(m_frustum, aabb);
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

cull_result_t cull(const cull_params_t &cull_params)
{
    CullVisitor cull_visitor(cull_params.camera, cull_params.check_intersection, cull_params.world_space);
    cull_params.scene->root()->accept(cull_visitor);
    cull_visitor.m_cull_result.scene = cull_params.scene;
    cull_visitor.m_cull_result.camera = cull_params.camera;
    return std::move(cull_visitor.m_cull_result);
}

}