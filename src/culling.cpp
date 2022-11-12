//
// Created by crocdialer on 6/14/20.
//

#include "vierkant/Visitor.hpp"
#include "vierkant/culling.hpp"

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
    for(const auto &t: obj_tags){ if(whitelist.count(t)){ return true; }}
    return whitelist.empty();
}

inline size_t vierkant::id_entry_key_hash_t::operator()(vierkant::id_entry_key_t const &key) const
{
    size_t h = 0;
    crocore::hash_combine(h, key.id);
    crocore::hash_combine(h, key.entry);
    return h;
}

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
            auto model_view = m_transform_stack.top() * object.transform;
            vierkant::MeshConstPtr mesh;

            // keep track of meshes
            if(object.has_component<vierkant::MeshPtr>())
            {
                mesh = object.get_component<vierkant::MeshPtr>();
                m_cull_result.meshes.insert(mesh);
            }

            // create drawables
            vierkant::create_drawables_params_t drawable_params = {};
            drawable_params.mesh = mesh;
            drawable_params.model_view = model_view;

            if(object.has_component<animation_state_t>())
            {
                auto &animation_state = object.get_component<animation_state_t>();
                drawable_params.animation_index = animation_state.index;
                drawable_params.animation_time = animation_state.current_time;
            }
            auto mesh_drawables = vierkant::create_drawables(drawable_params);

            for(uint32_t i = 0; i < mesh_drawables.size(); ++i)
            {
                auto &drawable = mesh_drawables[i];
                m_cull_result.entity_map[drawable.id] = object.id();
                drawable.matrices.projection = m_camera->projection_matrix();

                id_entry_key_t key = {object.id(), drawable.entry_index};
                m_cull_result.index_map[key] = i;
            }

            // move drawables into cull_result
            std::move(mesh_drawables.begin(), mesh_drawables.end(), std::back_inserter(m_cull_result.drawables));

            scoped_stack_push scoped_stack_push(m_transform_stack, m_transform_stack.top() * object.transform);
            for(Object3DPtr &child: object.children){ child->accept(*this); }
        }
    }

    bool should_visit(vierkant::Object3D &object) override
    {
        if(object.enabled && check_tags(m_tags, object.tags))
        {
            if(m_check_intersection)
            {
                // check intersection of aabb in eye-coords with view-frustum
                auto aabb = object.aabb().transform(m_transform_stack.top() * object.transform);
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