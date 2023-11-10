//
// Created by crocdialer on 6/14/20.
//

#include "vierkant/culling.hpp"
#include "vierkant/Visitor.hpp"
#include "vierkant/hash.hpp"

namespace vierkant
{

/**
 * @brief   Utility to check if one set of tags contains at least one item from another set.
 *
 * @param   whitelist the tags that shall pass the check.
 * @param   obj_tags    the tags to check against the whitelist
 * @return
 */
inline static bool check_tags(const std::set<std::string> &whitelist, const std::set<std::string> &obj_tags)
{
    for(const auto &t: obj_tags)
    {
        if(whitelist.count(t)) { return true; }
    }
    return whitelist.empty();
}

size_t vierkant::id_entry_key_hash_t::operator()(vierkant::id_entry_key_t const &key) const
{
    size_t h = 0;
    vierkant::hash_combine(h, key.id);
    vierkant::hash_combine(h, key.entry);
    return h;
}

struct scoped_stack_push
{
    std::stack<vierkant::transform_t> &stack;

    scoped_stack_push(std::stack<vierkant::transform_t> &stack, const vierkant::transform_t &t) : stack(stack)
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
            auto model_view = m_transform_stack.top() * object.transform;

            // keep track of meshes
            if(object.has_component<vierkant::mesh_component_t>())
            {
                const auto &mesh_component = object.get_component<vierkant::mesh_component_t>();
                m_cull_result.meshes.insert(mesh_component.mesh);

                // create drawables
                vierkant::create_drawables_params_t drawable_params = {};
                drawable_params.transform = model_view;

                if(object.has_component<animation_component_t>())
                {
                    auto &animation_state = object.get_component<animation_component_t>();
                    drawable_params.animation_index = animation_state.index;
                    drawable_params.animation_time = static_cast<float>(animation_state.current_time);
                }
                auto mesh_drawables = vierkant::create_drawables(mesh_component, drawable_params);

                for(uint32_t i = 0; i < mesh_drawables.size(); ++i)
                {
                    auto &drawable = mesh_drawables[i];
                    m_cull_result.entity_map[drawable.id] = object.id();
                    drawable.matrices.projection = m_camera->projection_matrix();

                    id_entry_key_t key = {object.id(), drawable.entry_index};
                    m_cull_result.index_map[key] = i + m_cull_result.drawables.size();
                }

                // move drawables into cull_result
                std::move(mesh_drawables.begin(), mesh_drawables.end(), std::back_inserter(m_cull_result.drawables));
            }
//            if(object.has_component<vierkant::model::lightsource_t>())
//            {
//                const auto &lightsource = object.get_component<vierkant::model::lightsource_t>();
//                m_cull_result.lights.push_back(vierkant::convert_light(lightsource));
//            }

            scoped_stack_push scoped_stack_push(m_transform_stack, m_transform_stack.top() * object.transform);
            for(Object3DPtr &child: object.children) { child->accept(*this); }
        }
    }

    bool should_visit(vierkant::Object3D &object) override
    {
        if(object.enabled && check_tags(m_tags, object.tags))
        {
            if(m_check_intersection)
            {
                // check intersection of aabb in eye-coords with view-frustum
                auto aabb = object.aabb().transform(mat4_cast(m_transform_stack.top() * object.transform));
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