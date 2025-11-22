//
// Created by crocdialer on 6/14/20.
//

#include "vierkant/culling.hpp"
#include "vierkant/Visitor.hpp"
#include "vierkant/hash.hpp"

namespace vierkant
{

struct scoped_stack_push
{
    std::stack<vierkant::transform_t> &stack;

    scoped_stack_push(std::stack<vierkant::transform_t> &stack_, const vierkant::transform_t &t) : stack(stack_)
    {
        stack.push(t);
    }

    ~scoped_stack_push() { stack.pop(); }
};

class CullVisitor : public vierkant::Visitor
{
public:
    CullVisitor(vierkant::CameraPtr cam, bool check_intersection, bool world_space)
        : m_frustum(cam->frustum()), m_camera(std::move(cam)), m_check_intersection(check_intersection)
    {
        if(!world_space) { m_transform_stack.push(m_camera->view_transform()); }
        else { m_transform_stack.push({}); }
    };

    void visit(vierkant::Object3D &object) override
    {
        if(should_visit(object))
        {
            auto model_view = object.transform ? m_transform_stack.top() * *object.transform : m_transform_stack.top();

            // keep track of meshes
            if(const auto *mesh_component = object.get_component_ptr<vierkant::mesh_component_t>())
            {
                m_cull_result.meshes.insert(mesh_component->mesh.get());

                // create drawables
                vierkant::create_drawables_params_t drawable_params = {};
                drawable_params.transform = model_view;

                if(object.has_component<animation_component_t>())
                {
                    auto &animation_state = object.get_component<animation_component_t>();
                    drawable_params.animation_index = animation_state.index;
                    drawable_params.animation_time = static_cast<float>(animation_state.current_time);
                }
                auto mesh_drawables = vierkant::create_drawables(*mesh_component, drawable_params);

                for(uint32_t i = 0; i < mesh_drawables.size(); ++i)
                {
                    auto &drawable = mesh_drawables[i];
                    m_cull_result.entity_map[drawable.id] = {object.id(), i};
                    drawable.matrices.projection = m_camera->projection_matrix();

                    id_entry_t key = {object.id(), drawable.entry_index};
                    m_cull_result.index_map[key] = m_cull_result.drawables.size();

                    m_cull_result.object_id_to_drawable_indices[object.id()].push_back(m_cull_result.drawables.size());

                    // move drawable into cull_result
                    m_cull_result.drawables.push_back(std::move(drawable));
                }
            }
            //            if(object.has_component<vierkant::model::lightsource_t>())
            //            {
            //                const auto &lightsource = object.get_component<vierkant::model::lightsource_t>();
            //                m_cull_result.lights.push_back(vierkant::convert_light(lightsource));
            //            }

            auto transform = object.transform ? m_transform_stack.top() * *object.transform : m_transform_stack.top();
            scoped_stack_push scoped_stack_push(m_transform_stack, transform);
            for(Object3DPtr &child: object.children) { child->accept(*this); }
        }
    }

    bool should_visit(vierkant::Object3D &object) const override
    {
        if(object.enabled && check_tags(m_tags, object.tags))
        {
            if(m_check_intersection)
            {
                // check intersection of aabb in eye-coords with view-frustum
                auto transform =
                        object.transform ? m_transform_stack.top() * *object.transform : m_transform_stack.top();
                auto aabb = object.aabb().transform(transform);
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

    std::stack<vierkant::transform_t> m_transform_stack;

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

}// namespace vierkant