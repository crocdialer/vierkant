#include <vierkant/culling.hpp>
#include <vierkant/physics_context.hpp>
#include <vierkant/physics_debug_draw.hpp>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Renderer/DebugRenderer.h>

namespace vierkant
{

SceneRenderer::render_result_t PhysicsDebugRenderer::render_scene(vierkant::Rasterizer &renderer,
                                                                  const vierkant::SceneConstPtr &scene,
                                                                  const vierkant::CameraPtr &cam,
                                                                  const std::set<std::string> &/*tags*/)
{
    auto physics_scene = std::dynamic_pointer_cast<const vierkant::PhysicsScene>(scene);
    if(!physics_scene) { return {}; }
    auto physics_debug_result = physics_scene->physics_context().debug_render();

    for(uint32_t i = 0; i < physics_debug_result.aabbs.size(); ++i)
    {
        const auto &aabb = physics_debug_result.aabbs[i];
        m_draw_context.draw_boundingbox(renderer, aabb, cam->view_transform(), cam->projection_matrix());

        const auto &[transform, geom] = physics_debug_result.triangle_meshes[i];
        auto &mesh = m_physics_meshes[geom];
        if(!mesh)
        {
            vierkant::Mesh::create_info_t mesh_create_info = {};
            mesh_create_info.mesh_buffer_params.use_vertex_colors = true;
            mesh = vierkant::Mesh::create_from_geometry(renderer.device(), geom, mesh_create_info);
            mesh->materials.front()->m.blend_mode = vierkant::BlendMode::Blend;
        }
        auto color = physics_debug_result.colors[i];
        color.w = 0.6f;
        m_draw_context.draw_mesh(renderer, mesh, cam->view_transform() * transform, cam->projection_matrix(),
                                 vierkant::ShaderType::UNLIT_COLOR, color, false, false);
    }


    if(physics_debug_result.lines)
    {
        m_draw_context.draw_lines(renderer, physics_debug_result.lines->positions, physics_debug_result.lines->colors,
                                  cam->view_transform(), cam->projection_matrix());
    }

    render_result_t ret = {};
    ret.num_draws = physics_debug_result.aabbs.size();
    return ret;
}

PhysicsDebugRenderer::PhysicsDebugRenderer(const vierkant::DevicePtr &device)
    : m_draw_context(device), m_pipeline_cache(vierkant::PipelineCache::create(device))
{}

PhysicsDebugRendererPtr PhysicsDebugRenderer::create(const vierkant::DevicePtr &device)
{
    return vierkant::PhysicsDebugRendererPtr(new PhysicsDebugRenderer(device));
}

}// namespace vierkant
