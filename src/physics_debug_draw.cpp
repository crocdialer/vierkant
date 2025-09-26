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
                                                                  const std::set<std::string> & /*tags*/)
{
    auto physics_scene = std::dynamic_pointer_cast<const vierkant::PhysicsScene>(scene);
    if(!physics_scene) { return {}; }
    auto physics_debug_result = physics_scene->physics_context().debug_render();

    // reference to previous frame-assets
    uint32_t last_index = (m_rasterizer.current_index() - 1 + m_rasterizer.num_concurrent_frames()) %
                          m_rasterizer.num_concurrent_frames();
    auto &last_frame_context = m_frame_contexts[last_index];

    // reference to current frame-assets
    auto &frame_context = m_frame_contexts[m_rasterizer.current_index()];
    frame_context.semaphore.wait(frame_context.current_semaphore_value);
    frame_context.current_semaphore_value++;
    frame_context.settings = settings;

    frame_context.physics_meshes.clear();
    const auto &last_physics_meshes = last_frame_context.physics_meshes;

    for(uint32_t i = 0; i < physics_debug_result.aabbs.size(); ++i)
    {
        if(frame_context.settings.draw_aabbs)
        {
            const auto &aabb = physics_debug_result.aabbs[i];
            m_draw_context.draw_boundingbox(m_rasterizer, aabb, cam->view_transform(), cam->projection_matrix());
        }

        if(frame_context.settings.draw_meshes)
        {
            const auto &[transform, geom] = physics_debug_result.triangle_meshes[i];
            vierkant::MeshPtr mesh;

            // search last
            auto last_mesh_it = last_physics_meshes.find(geom.get());
            if(last_mesh_it != last_physics_meshes.end()) { mesh = last_mesh_it->second; }
            else
            {
                // search current
                auto mesh_it = frame_context.physics_meshes.find(geom.get());
                if(mesh_it != frame_context.physics_meshes.end()) { mesh = mesh_it->second; }
            }

            if(!mesh)
            {
                vierkant::Mesh::create_info_t mesh_create_info = {};
                mesh_create_info.mesh_buffer_params.use_vertex_colors = true;
                mesh = vierkant::Mesh::create_from_geometry(m_rasterizer.device(), geom, mesh_create_info);
                mesh->materials.front()->m.blend_mode = vierkant::BlendMode::Blend;
                frame_context.physics_meshes[geom.get()] = mesh;
            }
            auto color = settings.use_mesh_colors ? physics_debug_result.colors[i] : glm::vec4(1.f);
            m_draw_context.draw_mesh(m_rasterizer, mesh, cam->view_transform() * transform, cam->projection_matrix(),
                                     vierkant::ShaderType::UNLIT_COLOR, color, true, true);
        }
    }

    if(frame_context.settings.draw_lines && physics_debug_result.lines)
    {
        m_draw_context.draw_lines(m_rasterizer, physics_debug_result.lines->positions,
                                  physics_debug_result.lines->colors, cam->view_transform(), cam->projection_matrix());
    }

    auto cmd_buf = m_rasterizer.render(frame_context.frame_buffer);

    vierkant::semaphore_submit_info_t signal_info = {};
    signal_info.semaphore = frame_context.semaphore.handle();
    signal_info.signal_value = frame_context.current_semaphore_value;
    signal_info.signal_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    frame_context.frame_buffer.submit({cmd_buf}, m_queue, {signal_info});

    // draw overlay-image
    m_draw_context.draw_image_fullscreen(renderer, frame_context.frame_buffer.color_attachment(),
                                         frame_context.frame_buffer.depth_attachment(), false, true,
                                         frame_context.settings.overlay_color, 0.01f);

    vierkant::semaphore_submit_info_t wait_info = {};
    wait_info.semaphore = frame_context.semaphore.handle();
    wait_info.wait_value = frame_context.current_semaphore_value;
    wait_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    render_result_t ret = {};
    ret.semaphore_infos = {wait_info};
    ret.num_draws = physics_debug_result.aabbs.size();
    return ret;
}

PhysicsDebugRenderer::PhysicsDebugRenderer(const create_info_t &create_info)
    : m_draw_context(create_info.device), m_pipeline_cache(create_info.pipeline_cache), m_queue(create_info.queue)
{
    vierkant::debug_label_t debug_label = {"physics debug"};
    vierkant::Rasterizer::create_info_t raster_info = {};
    raster_info.viewport.width = static_cast<float>(create_info.settings.resolution.x);
    raster_info.viewport.height = static_cast<float>(create_info.settings.resolution.y);
    raster_info.indirect_draw = true;
    raster_info.pipeline_cache = create_info.pipeline_cache;
    raster_info.num_frames_in_flight = create_info.num_frames_in_flight;
    raster_info.debug_label = debug_label;
    m_rasterizer = vierkant::Rasterizer(create_info.device, raster_info);

    // albedo / ao_rough_metal
    Image::Format color_format = {};
    color_format.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    color_format.initial_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    color_format.name = "color";

    Image::Format depth_format = {};
    depth_format.format = VK_FORMAT_D32_SFLOAT;
    depth_format.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_format.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depth_format.initial_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depth_format.name = "depth";

    vierkant::Framebuffer::create_info_t frame_buffer_info = {};
    frame_buffer_info.size = {create_info.settings.resolution.x, create_info.settings.resolution.y, 1};
    frame_buffer_info.queue = m_queue;
    frame_buffer_info.color_attachment_format = color_format;
    frame_buffer_info.depth_attachment_format = depth_format;
    frame_buffer_info.depth = true;
    frame_buffer_info.debug_label = debug_label;

    m_frame_contexts.resize(create_info.num_frames_in_flight);
    for(auto &asset: m_frame_contexts)
    {
        asset.semaphore = Semaphore(create_info.device);
        asset.frame_buffer = vierkant::Framebuffer(create_info.device, frame_buffer_info);
        asset.frame_buffer.clear_color = glm::vec4(0.f);
        asset.frame_buffer.debug_label = debug_label;
    }
}

PhysicsDebugRendererPtr PhysicsDebugRenderer::create(const create_info_t &create_info)
{
    return vierkant::PhysicsDebugRendererPtr(new PhysicsDebugRenderer(create_info));
}

}// namespace vierkant
