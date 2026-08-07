//
// Created by crocdialer on 6/14/20.
//

#include <utility>

#include "vierkant/Visitor.hpp"
#include "vierkant/culling.hpp"
#include "vierkant/hash.hpp"

namespace vierkant
{

class CullVisitor : public vierkant::Visitor
{
public:
    CullVisitor(vierkant::SceneConstPtr scene, vierkant::Object3DPtr cam, bool check_intersection, bool world_space)
        : m_frustum(camera::frustum(cam.get())), m_camera(std::move(cam)), m_scene(std::move(scene)),
          m_check_intersection(check_intersection)
    {
        if(!world_space) { m_base_transform = camera::view_transform(m_camera.get()); }
    };

    void visit(vierkant::Object3D &object) override
    {
        if(object.enabled && check_tags(m_tags, object.tags))
        {
            // cached global, no accumulation during traversal required
            const auto model_view = m_base_transform * object.global_transform();

            if(m_check_intersection)
            {
                // check intersection of aabb in eye-coords with view-frustum
                auto aabb = object.aabb().transform(model_view);
                if(!vierkant::intersect(m_frustum, aabb)) { return; }
            }

            // keep track of meshes
            if(const auto *mesh_component = object.get_component_ptr<vierkant::mesh_component_t>())
            {
                m_cull_result.meshes.insert(mesh_component->mesh.get());

                // create drawables
                vierkant::create_mesh_drawables_params_t drawable_params = {};
                drawable_params.assets = m_scene->asset_provider().get();
                drawable_params.transform = model_view;

                if(object.has_component<animation_component_t>())
                {
                    const auto &animation_state = object.get_component<animation_component_t>();
                    drawable_params.animation_index = animation_state.index;
                    drawable_params.animation_time = static_cast<float>(animation_state.current_time);
                }
                auto mesh_drawables = vierkant::create_mesh_drawables(*mesh_component, drawable_params);

                for(uint32_t i = 0; i < mesh_drawables.size(); ++i)
                {
                    auto &drawable = mesh_drawables[i];
                    m_cull_result.entity_map[drawable.id] = {.id = object.id(), .entry = i};
                    drawable.matrices.projection = camera::projection_matrix(m_camera.get());

                    id_entry_t key = {object.id(), drawable.entry_index};
                    m_cull_result.index_map[key] = m_cull_result.drawables.size();

                    m_cull_result.object_id_to_drawable_indices[object.id()].push_back(m_cull_result.drawables.size());

                    // move drawable into cull_result
                    m_cull_result.drawables.push_back(std::move(drawable));
                }
            }

            for(const Object3DPtr &child: object.children) { child->accept(*this); }
        }
    }

    std::set<std::string> m_tags;

    vierkant::Frustum m_frustum;

    vierkant::Object3DPtr m_camera;

    vierkant::SceneConstPtr m_scene;

    bool m_check_intersection;

    //! view-transform for eye-space culling, identity when culling in world-space
    vierkant::transform_t m_base_transform = {};

    cull_result_t m_cull_result;
};

cull_result_t cull(const cull_params_t &cull_params)
{
    CullVisitor cull_visitor(cull_params.scene, cull_params.camera, cull_params.check_intersection,
                             cull_params.world_space);
    cull_params.scene->root()->accept(cull_visitor);
    cull_visitor.m_cull_result.scene = cull_params.scene;
    cull_visitor.m_cull_result.camera = cull_params.camera;
    cull_visitor.m_cull_result.lights = gather_lights(cull_params.scene, cull_params.tags);
    return std::move(cull_visitor.m_cull_result);
}

std::vector<vierkant::light_t> gather_lights(const vierkant::SceneConstPtr &scene, const std::set<std::string> &tags)
{
    std::vector<vierkant::light_t> ret;

    vierkant::LambdaVisitor visitor;
    visitor.traverse(*scene->root(), [&ret, &scene, &tags](const Object3D &object) -> bool {
        if(!object.enabled || !check_tags(tags, object.tags)) { return false; }

        if(const auto *light_cmp = object.get_component_ptr<vierkant::lightsource_component_t>())
        {
            const auto *light_src = scene->asset_provider()->light(light_cmp->light_id);

            // rasterizers shade punctual lights only, area-lights remain path-tracer exclusive
            if(light_src && light_src->intensity > 0.f && is_punctual(light_src->type))
            {
                ret.push_back(vierkant::convert_light(*light_src, object.global_transform()));
            }
        }
        return true;
    });
    return ret;
}

}// namespace vierkant