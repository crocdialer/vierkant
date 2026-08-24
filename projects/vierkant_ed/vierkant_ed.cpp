#include <crocore/Image.hpp>

#include <vierkant/NoiseGenerator.hpp>
#include <vierkant/PBRDeferred.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/cubemap_utils.hpp>

#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "vierkant_ed.hpp"

#include <ranges>

using log_delegate_fn_t = std::function<void(const std::string &msg, spdlog::level::level_enum log_level,
                                             const std::string &logger_name)>;

class delegate_sink_t : public spdlog::sinks::base_sink<std::mutex>
{
public:
    std::unordered_map<std::string, log_delegate_fn_t> log_delegates;

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override
    {
        // log_msg is a struct containing the log entry info like level, timestamp, thread id etc.
        // msg.raw contains pre formatted log

        // If needed (very likely but not mandatory), the sink formats the message before sending it to its final destination:
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);

        // bounce out via delegates
        for(const auto &[name, delegate]: log_delegates)
        {
            if(delegate)
            {
                delegate(fmt::to_string(formatted), msg.level,
                         std::string(msg.logger_name.begin(), msg.logger_name.end()));
            }
        }
    }

    void flush_() override {}
};

// gcc15 acting up weirdly here
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
VierkantEd::VierkantEd(const crocore::Application::create_info_t &create_info) : crocore::Application(create_info)
{
    // try to read settings
    if(auto settings = load_settings()) { m_settings = std::move(*settings); }
    else
    {
        // initial pos
        m_settings.orbit_camera->spherical_coords = {-0.5f, 1.1f};
        m_settings.orbit_camera->distance = 4.f;
    }
    this->loop_throttling = !m_settings.window_info.vsync;
    this->target_loop_frequency = m_settings.target_fps;

    m_scene->physics_context().mesh_provider = m_scene->asset_provider()->mesh_provider();
#ifndef NDEBUG
    m_settings.use_validation = true;
#endif
}
// gcc15 acting up weirdly here
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void VierkantEd::setup()
{
    init_logger();

    create_context_and_window();

    // create ui and inputs
    create_ui();

    create_texture_image();
    create_graphics_pipeline();

    // load a scene (keep a CLI-provided top-scene key; default only when none was staged)
    if(!m_scene_paths.contains(m_scene_id)) { m_scene_paths[m_scene_id] = s_default_scene_path; }
    auto scene_data =
            m_scene_data.nodes.empty() && m_scene_data.environment_path.empty() ? load_scene_data() : m_scene_data;
    build_scene(scene_data, true, m_scene_id);
}

void VierkantEd::teardown()
{
    spdlog::debug("joining background tasks ...");
    background_queue().join_all();
    main_queue().poll();

    // clear scene, free referenced gpu-resources
    m_scene.reset();

    m_device->wait_idle();
    spdlog::info("ciao {}", name());
}

void VierkantEd::poll_events()
{
    if(m_window) { m_window->poll_events(); }
}

void VierkantEd::create_context_and_window()
{
    vierkant::Instance::create_info_t instance_info = {};
    instance_info.extensions = vierkant::Window::required_extensions();
    instance_info.use_validation_layers = m_settings.use_validation;
    instance_info.use_debug_labels = m_settings.use_debug_labels;
    m_instance = vierkant::Instance(instance_info);

    m_settings.window_info.title = name();
    m_settings.window_info.instance = m_instance.handle();
    m_window = vierkant::Window::create(m_settings.window_info);

    VkPhysicalDevice physical_device = m_instance.physical_devices().front();

    for(const auto &pd: m_instance.physical_devices())
    {
        VkPhysicalDeviceProperties2 device_props = vierkant::device_properties(pd);

        if(device_props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            physical_device = pd;
            break;
        }
    }
    spdlog::info(vierkant::device_info(physical_device));

    // create device
    vierkant::Device::create_info_t device_info = {};
    device_info.instance = m_instance.handle();
    device_info.physical_device = physical_device;
    device_info.use_validation = m_instance.use_validation_layers();
    device_info.direct_function_pointers = true;
    device_info.surface = m_window->surface();

    // limit number of queues
    device_info.max_num_queues = 4;

    // check raytracing-pipeline support
    m_settings.enable_raytracing_pipeline_features =
            m_settings.enable_raytracing_pipeline_features &&
            vierkant::check_device_extension_support(physical_device, vierkant::RayTracer::required_extensions());

    // add the raytracing-extensions
    if(m_settings.enable_raytracing_pipeline_features)
    {
        device_info.extensions = vierkant::RayTracer::required_extensions();
    }

    // check ray-query support
    m_settings.enable_ray_query_features =
            m_settings.enable_ray_query_features &&
            vierkant::check_device_extension_support(physical_device, vierkant::RayBuilder::required_extensions()) &&
            vierkant::check_device_extension_support(physical_device, {VK_KHR_RAY_QUERY_EXTENSION_NAME});

    // add the raytracing-extensions
    if(m_settings.enable_ray_query_features)
    {
        for(const auto &ext: vierkant::RayBuilder::required_extensions()) { device_info.extensions.push_back(ext); }
        device_info.extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }

    // check mesh-shader support
    m_settings.enable_mesh_shader_device_features =
            m_settings.enable_mesh_shader_device_features &&
            vierkant::check_device_extension_support(physical_device, {VK_EXT_MESH_SHADER_EXTENSION_NAME});

    if(m_settings.enable_mesh_shader_device_features)
    {
        device_info.extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    }

    // NOTE: those extensions can be used, but not widely supported and our implementation is experimental
    //    device_info.extensions.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    if(vierkant::check_device_extension_support(physical_device, {VK_EXT_OPACITY_MICROMAP_EXTENSION_NAME}))
    {
        device_info.extensions.push_back(VK_EXT_OPACITY_MICROMAP_EXTENSION_NAME);
    }

    m_device = vierkant::Device::create(device_info);
    m_window->create_swapchain(m_device, std::min(m_device->max_usable_samples(), m_settings.window_info.sample_count),
                               m_settings.window_info.vsync, m_settings.window_info.use_hdr);

    // create a WindowDelegate
    vierkant::window_delegate_t window_delegate = {};
    window_delegate.draw_fn = [this](const vierkant::WindowPtr &w) { return draw(w); };
    window_delegate.resize_fn = [this](uint32_t w, uint32_t h) {
        VkViewport viewport = {0.f, 0.f, static_cast<float>(w), static_cast<float>(h), 0.f, 1.f};
        m_renderer.viewport = m_renderer_overlay.viewport = m_renderer_gui.viewport = viewport;
        m_renderer.sample_count = m_renderer_overlay.sample_count = m_renderer_gui.sample_count =
                m_window->swapchain().sample_count();
        m_camera_control.current->screen_size = {w, h};

        auto *cam_cmp = m_render_camera->get_component_ptr<vierkant::camera_component_t>();
        assert(cam_cmp);

        cam_cmp->physical.aspect = m_window->aspect_ratio();
    };
    window_delegate.close_fn = [this]() { running = false; };
    m_window->window_delegates[name()] = window_delegate;

    // create a draw context
    m_draw_context = vierkant::DrawContext(m_device);

    m_pipeline_cache = vierkant::PipelineCache::create(m_device);

    // set some separate queues for background stuff
    uint32_t i = 1;
    auto num_queues = m_device->queues(vierkant::Device::Queue::GRAPHICS).size();
    m_queue_model_loading = m_device->queues(vierkant::Device::Queue::GRAPHICS)[i++ % num_queues].queue;
    m_queue_image_loading = m_device->queues(vierkant::Device::Queue::GRAPHICS)[i++ % num_queues].queue;
    m_queue_render = m_device->queues(vierkant::Device::Queue::GRAPHICS)[i++ % num_queues].queue;

    // buffer-flags for mesh-buffers
    m_mesh_buffer_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
}

void VierkantEd::create_graphics_pipeline()
{
    m_pipeline_cache->clear();

    bool use_raytracer = m_scene_renderer ? m_scene_renderer == m_path_tracer : m_settings.path_tracing;

    const auto &framebuffers = m_window->swapchain().framebuffers();
    auto fb_extent = framebuffers.front().extent();

    vierkant::Rasterizer::create_info_t create_info = {};
    create_info.num_frames_in_flight = framebuffers.size();
    create_info.sample_count = m_window->swapchain().sample_count();
    create_info.viewport.width = static_cast<float>(fb_extent.width);
    create_info.viewport.height = static_cast<float>(fb_extent.height);
    create_info.viewport.maxDepth = static_cast<float>(fb_extent.depth);
    create_info.pipeline_cache = m_pipeline_cache;

    m_renderer = vierkant::Rasterizer(m_device, create_info);
    m_renderer_overlay = vierkant::Rasterizer(m_device, create_info);
    m_renderer_overlay.indirect_draw = true;

    m_renderer_gui = vierkant::Rasterizer(m_device, create_info);
    m_renderer_gui.debug_label = {.text = "imgui"};

    vierkant::PBRDeferred::create_info_t pbr_render_info = {};
    pbr_render_info.queue = m_queue_render;
    pbr_render_info.num_frames_in_flight = framebuffers.size();
    pbr_render_info.hdr_format = m_hdr_format;
    pbr_render_info.pipeline_cache = m_pipeline_cache;
    pbr_render_info.settings = m_settings.pbr_settings;
    pbr_render_info.settings.use_ray_queries &= m_settings.enable_ray_query_features;
    pbr_render_info.logger_name = "pbr_deferred";

    if(m_pbr_renderer)
    {
        const auto &prev_images = m_pbr_renderer->image_bundle();
        pbr_render_info.conv_lambert = prev_images.environment_diffuse;
        pbr_render_info.conv_ggx = prev_images.environment_specular;
        pbr_render_info.brdf_lut = prev_images.bsdf_lut;
        pbr_render_info.settings = m_pbr_renderer->settings;
    }
    const auto &fallback_env = m_environment_texture;

    if(!pbr_render_info.conv_lambert)
    {
        constexpr uint32_t lambert_size = 128;
        pbr_render_info.conv_lambert = vierkant::create_convolution_lambert(m_device, fallback_env, lambert_size,
                                                                            m_hdr_format, m_queue_image_loading);
    }
    if(!pbr_render_info.conv_ggx)
    {
        pbr_render_info.conv_ggx = fallback_env;
        pbr_render_info.conv_ggx = vierkant::create_convolution_ggx(m_device, fallback_env, fallback_env->width(),
                                                                    m_hdr_format, m_queue_image_loading);
    }
    pbr_render_info.conv_lambert->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);
    pbr_render_info.conv_ggx->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);

    m_pbr_renderer = vierkant::PBRDeferred::create(m_device, pbr_render_info);

    if(m_settings.enable_raytracing_pipeline_features)
    {
        vierkant::PBRPathTracer::create_info_t path_tracer_info = {};
        path_tracer_info.num_frames_in_flight = framebuffers.size();
        path_tracer_info.pipeline_cache = m_pipeline_cache;

        path_tracer_info.settings = m_path_tracer ? m_path_tracer->settings : m_settings.path_tracer_settings;
        path_tracer_info.queue = m_queue_render;

        m_path_tracer = vierkant::PBRPathTracer::create(m_device, path_tracer_info);

        // hand the scene-level OMM cache (non-owning) to the path-tracer/RayBuilder
        m_path_tracer->settings.omm_cache = &m_scene_omm_cache;
    }

    if(use_raytracer && m_path_tracer) { m_scene_renderer = m_path_tracer; }
    else
    {
        m_scene_renderer = m_pbr_renderer;
    }

    // object-overlay assets per frame
    m_overlay_assets.resize(framebuffers.size());
    for(auto &overlay_asset: m_overlay_assets)
    {
        overlay_asset.semaphore = vierkant::Semaphore(m_device);
        overlay_asset.command_buffer = vierkant::CommandBuffer(m_device, m_device->command_pool_transient());
        overlay_asset.object_overlay_context =
                vierkant::create_object_overlay_context(m_device, glm::vec2(pbr_render_info.settings.resolution) / 2.f);
    }

    // buffer-flags for mesh-buffers
    m_mesh_buffer_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if(m_settings.enable_raytracing_pipeline_features)
    {
        m_mesh_buffer_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }

    // physics debug-drawing
    vierkant::PhysicsDebugRenderer::create_info_t physics_debug_info = {};
    physics_debug_info.device = m_device;
    physics_debug_info.queue = m_queue_render;
    physics_debug_info.num_frames_in_flight = framebuffers.size();
    physics_debug_info.pipeline_cache = m_pipeline_cache;
    m_physics_debug = vierkant::PhysicsDebugRenderer::create(physics_debug_info);
}

void VierkantEd::create_texture_image()
{
    //    // try to fetch cool image
    //    auto http_response = netzer::http::get(g_texture_url);

    crocore::ImagePtr img;
    vierkant::Image::Format fmt;

    //    // create from downloaded data
    //    if(!http_response.data.empty()) { img = crocore::create_image_from_data(http_response.data, 4); }
    //    else
    {
        // create 4x4 black/white checkerboard image
        uint32_t v[] = {0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF,
                        0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF};

        img = crocore::Image_<uint8_t>::create(reinterpret_cast<uint8_t *>(v), 4, 4, 4);
        fmt.sampler_state.mag_filter = VK_FILTER_NEAREST;
        fmt.format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    fmt.extent = {img->width(), img->height(), 1};
    fmt.use_mipmap = true;
    m_primitive_texture = vierkant::Image::create(m_device, img->data(), fmt);
    m_environment_texture = vierkant::cubemap_neutral_environment(m_device, 256, m_device->queue(), true, m_hdr_format);
    m_scene->set_environment(m_environment_texture);
    m_scene->asset_provider()->add_texture({m_primitive_texture_id, vierkant::SamplerId::nil()}, m_primitive_texture);

    m_primitive_material.name = "primitive_material";
    m_primitive_material.id = vierkant::MaterialId::from_name(m_primitive_material.name);
    m_primitive_material.texture_data[vierkant::TextureType::Color].texture_id = m_primitive_texture_id;

    // primitive-meshes are created lazily by the asset-provider, using our device + canonical buffer-params
    m_scene->asset_provider()->set_mesh_factory([this](const vierkant::GeometryConstPtr &geom) {
        vierkant::Mesh::create_info_t mesh_create_info = {};
        mesh_create_info.mesh_buffer_params = m_settings.mesh_buffer_params;
        mesh_create_info.buffer_usage_flags = m_mesh_buffer_flags;
        return vierkant::Mesh::create_from_geometry(m_device, geom, mesh_create_info);
    });

    for(const auto &prim_name: vierkant::AssetProvider::primitive_names() | std::views::values)
    {
        m_model_paths[vierkant::MeshId::from_name(prim_name)] = prim_name;
    }

    // test noise-gen
    auto noisegen = vierkant::NoiseGenerator::create(m_device, {.size = {1024, 1024, 1}});
    vierkant::CommandBuffer cmd_buf(m_device, m_device->command_pool_transient());
    cmd_buf.begin();
    m_noise_texture = noisegen->generate(cmd_buf.handle(), glm::vec2(2.f), 0.f);
    cmd_buf.submit(m_queue_image_loading, true);
    m_scene->asset_provider()->add_texture({m_noise_texture_id, vierkant::SamplerId::nil()}, m_noise_texture);
}

//! first object carrying a camera-component below 'root', excluding 'root' itself
static vierkant::Object3D *find_eye(vierkant::Object3D *root)
{
    vierkant::SelectVisitor<vierkant::Object3D> visitor(vierkant::LAYER_ALL, false);
    root->accept(visitor);

    for(auto *obj: visitor.objects)
    {
        if(obj != root && obj->has_component<vierkant::camera_component_t>()) { return obj; }
    }
    return nullptr;
}

void VierkantEd::update_player_input(double time_delta)
{
    m_player_control->update(time_delta);

    vierkant::SelectVisitor<vierkant::Object3D> visitor(vierkant::LAYER_ALL, false);
    m_scene->root()->accept(visitor);

    for(auto *obj: visitor.objects)
    {
        auto *phys_cmp = obj->get_component_ptr<vierkant::physics_component_t>();
        if(!phys_cmp || !phys_cmp->character) { continue; }

        auto &character = *phys_cmp->character;
        m_player_control->apply(character);

        // the view-rotation is authoritative and each axis lands on exactly one transform.
        // yaw is either taken by the body or by the eye, never both, or a parented eye rotates twice.
        const glm::quat yaw_rotation(glm::vec3(0.f, character.yaw, 0.f));
        const glm::quat pitch_rotation(glm::vec3(character.pitch, 0.f, 0.f));

        if(m_settings.body_use_view_yaw)
        {
            // the physics-readback owns the object-transform, so the body has to be rotated
            auto &body_interface = m_scene->physics_context().body_interface();
            if(vierkant::transform_t transform; body_interface.get_transform(obj->id(), transform))
            {
                transform.rotation = yaw_rotation;
                body_interface.set_transform(obj->id(), transform);
            }
        }

        // the eye is attached manually, keep whatever offset it was given
        if(auto *eye = find_eye(obj))
        {
            const auto *eye_transform = eye->transform();
            auto transform = eye_transform ? *eye_transform : vierkant::transform_t{};
            transform.rotation = m_settings.body_use_view_yaw ? pitch_rotation : yaw_rotation * pitch_rotation;
            eye->set_transform(transform);
        }

        // with a drift-budget set, the path-tracer sizes its own window from the camera-motion
        if(m_path_tracer && m_path_tracer->settings.max_accumulation_drift <= 0.f)
        {
            m_path_tracer->reset_accumulator();
        }
        break;
    }
}

void VierkantEd::update(double time_delta)
{
    // camera-controls run before the scene-update: the player-controller's input is turned into
    // forces there, applying it afterwards would be one frame late
    m_camera_control.current->update(time_delta);
    if(m_settings.character_input) { update_player_input(time_delta); }

    // animations and the simulation are paced independently, both off the real frame-time
    m_scene->animation_speed = m_settings.animation_playback ? m_settings.playback_speed : 0.0;
    m_scene->simulation_playback = m_settings.physics_playback;

    // update animated objects, step the simulation, clear flags
    m_scene->update(time_delta);

    if(m_settings.draw_ui) { m_gui_context.update(time_delta, m_window->size(), m_window->framebuffer_size()); }

    update_js(time_delta);

    // issue top-level draw-command
    m_window->draw();
}

vierkant::window_delegate_t::draw_result_t VierkantEd::draw(const vierkant::WindowPtr & /*w*/)
{
    const auto &framebuffer = m_window->swapchain().current_framebuffer();
    std::vector<vierkant::semaphore_submit_info_t> semaphore_infos;

    // tmp testing of overlay-drizzling
    auto &overlay_assets = m_overlay_assets[m_renderer_overlay.current_index()];

    auto render_scene = [this, &framebuffer, &semaphore_infos, &overlay_assets]() -> VkCommandBuffer {
        auto render_result =
                m_scene_renderer->render_scene(m_renderer, m_scene, m_render_camera, ~vierkant::LAYER_EDITOR);
        auto overlay_submit_info = generate_overlay(overlay_assets, render_result.object_ids);
        {
            std::unique_lock lock(m_mutex_semaphore_submit);
            semaphore_infos.insert(semaphore_infos.end(), render_result.semaphore_infos.begin(),
                                   render_result.semaphore_infos.end());
            semaphore_infos.push_back(overlay_submit_info);
        }
        overlay_assets.object_by_index_fn = render_result.object_by_index_fn;
        overlay_assets.indices_by_id_fn = render_result.indices_by_id_fn;
        return m_renderer.render(framebuffer);
    };

    auto render_scene_overlays = [this, &framebuffer, &semaphore_infos, selected_objects = m_selected_objects,
                                  &overlay_assets]() -> VkCommandBuffer {
        // draw silhouette/mask for selected indices
        m_draw_context.draw_image(m_renderer_overlay, overlay_assets.overlay, {}, glm::vec4(.8f, .5f, .1f, .7f));

        // physics debug overlay
        if(m_settings.draw_physics)
        {
            auto render_result = m_physics_debug->render_scene(m_renderer_overlay, m_scene, m_render_camera,
                                                               ~vierkant::LAYER_EDITOR);
            std::unique_lock lock(m_mutex_semaphore_submit);
            semaphore_infos.insert(semaphore_infos.end(), render_result.semaphore_infos.begin(),
                                   render_result.semaphore_infos.end());
        }

        auto view_transform = vierkant::camera::view_transform(m_render_camera.get());
        auto cam_projection = vierkant::camera::projection_matrix(m_render_camera.get());

        for(const auto &obj: selected_objects)
        {
            auto modelview = view_transform * obj->global_transform();

            if(m_settings.draw_aabbs)
            {
                m_draw_context.draw_boundingbox(m_renderer_overlay, obj->aabb(), modelview, cam_projection);

                auto sub_aabbs = obj->sub_aabbs();

                for(const auto &aabb: sub_aabbs)
                {
                    m_draw_context.draw_boundingbox(m_renderer_overlay, aabb, modelview, cam_projection);
                }
            }

            if(m_settings.draw_node_hierarchy)
            {
                vierkant::nodes::node_animation_t animation = {};

                if(obj->has_component<vierkant::animation_component_t>())
                {
                    const auto &mesh = obj->get_component<vierkant::mesh_component_t>().mesh;

                    auto &animation_state = obj->get_component<vierkant::animation_component_t>();
                    animation = mesh->node_animations[animation_state.index];
                    auto node = mesh->root_bone ? mesh->root_bone : mesh->root_node;
                    m_draw_context.draw_node_hierarchy(m_renderer_overlay, node, animation,
                                                       static_cast<float>(animation_state.current_time), modelview,
                                                       cam_projection);
                }
            }
        }

        if(m_settings.draw_grid)
        {
            const auto &cam_cmp = m_render_camera->get_component<vierkant::camera_component_t>();
            bool is_ortho = cam_cmp.projection == vierkant::camera_component_t::ORTHO;

            m_draw_context.draw_grid(m_renderer_overlay, glm::vec4(glm::vec3(0.8f), 1.f), 1.f, glm::vec2(0.05f),
                                     is_ortho, true, view_transform, cam_projection);
        }

        if(m_selection_area)
        {
            m_draw_context.draw_rect(m_renderer_overlay, *m_selection_area, glm::vec4(.3f, 0.25f, .8f, 0.3f));
        }

        return m_renderer_overlay.render(framebuffer);
    };

    auto render_gui = [this, &framebuffer]() -> VkCommandBuffer {
        m_gui_context.draw_gui(m_renderer_gui);
        return m_renderer_gui.render(framebuffer);
    };

    vierkant::window_delegate_t::draw_result_t ret;

    // submit and wait for all command-creation tasks to complete
    std::vector<std::future<VkCommandBuffer>> cmd_futures;
    cmd_futures.push_back(background_queue().post<crocore::ThreadPoolClassic::Priority::High>(render_scene));
    cmd_futures.push_back(background_queue().post<crocore::ThreadPoolClassic::Priority::High>(render_scene_overlays));
    if(m_settings.draw_ui)
    {
        cmd_futures.push_back(background_queue().post<crocore::ThreadPoolClassic::Priority::High>(render_gui));
    }
    crocore::wait_all(cmd_futures);

    // get values from completed futures
    for(auto &f: cmd_futures)
    {
        VkCommandBuffer commandbuffer = f.get();
        if(commandbuffer) { ret.command_buffers.push_back(commandbuffer); }
    }

    // get semaphore infos
    ret.semaphore_infos = std::move(semaphore_infos);
    return ret;
}

vierkant::semaphore_submit_info_t VierkantEd::generate_overlay(VierkantEd::overlay_assets_t &overlay_asset,
                                                              const vierkant::ImagePtr &id_img)
{
    constexpr uint64_t overlay_semaphore_done = 1;
    overlay_asset.semaphore.wait(overlay_asset.semaphore_value);
    overlay_asset.semaphore_value += overlay_semaphore_done;

    overlay_asset.command_buffer.begin(0);

    vierkant::object_overlay_params_t overlay_params = {};
    overlay_params.mode = m_settings.object_overlay_mode;
    overlay_params.commandbuffer = overlay_asset.command_buffer.handle();
    overlay_params.object_id_img = id_img;

    if(overlay_asset.indices_by_id_fn)
    {
        vierkant::LambdaVisitor visitor;

        for(auto &obj: m_selected_objects)
        {
            visitor.traverse(*obj, [&overlay_asset, &overlay_params](const auto &obj) -> bool {
                auto draw_indices = overlay_asset.indices_by_id_fn(obj.id());
                overlay_params.object_ids.insert(draw_indices.begin(), draw_indices.end());
                return true;
            });
        }
    }

    overlay_asset.overlay = vierkant::object_overlay(overlay_asset.object_overlay_context, overlay_params);

    vierkant::semaphore_submit_info_t overlay_signal_info = {};
    overlay_signal_info.semaphore = overlay_asset.semaphore.handle();
    overlay_signal_info.signal_value = overlay_asset.semaphore_value + overlay_semaphore_done;
    overlay_signal_info.signal_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    overlay_asset.command_buffer.submit(m_queue_render, false, VK_NULL_HANDLE, {overlay_signal_info});

    vierkant::semaphore_submit_info_t overlay_wait_info = {};
    overlay_wait_info.semaphore = overlay_asset.semaphore.handle();
    overlay_wait_info.wait_value = overlay_asset.semaphore_value + overlay_semaphore_done;
    overlay_wait_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    m_object_id_image = id_img;
    return overlay_wait_info;
}

void VierkantEd::init_logger()
{
    spdlog::set_level(m_settings.log_level);

    // create logger for renderers
    constexpr char pbr_logger_name[] = "pbr_deferred";
    _loggers[pbr_logger_name] = spdlog::stdout_color_mt(pbr_logger_name);
    _loggers[spdlog::default_logger()->name()] = spdlog::default_logger();

    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink;
    if(!m_settings.log_file.empty())
    {
        try
        {
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(m_settings.log_file);
        } catch(spdlog::spdlog_ex &e) {}
    }

    auto scroll_log_sink = std::make_shared<delegate_sink_t>();
    scroll_log_sink->log_delegates[name()] = [this](const std::string &msg, spdlog::level::level_enum log_level,
                                                    const std::string & /*logger_name*/) {
        std::unique_lock lock(m_log_queue_mutex);
        m_log_queue.emplace_back(msg, log_level);
        while(m_log_queue.size() > m_max_log_queue_size) { m_log_queue.pop_front(); }
    };
    for(auto &logger: _loggers | std::views::values)
    {
        logger->sinks().push_back(scroll_log_sink);
        if(file_sink) { logger->sinks().push_back(file_sink); }
    }
}

int main(int argc, char *argv[])
{
    crocore::Application::create_info_t create_info = {};
    create_info.arguments = {argv, argv + argc};
    create_info.num_background_threads = std::max<uint32_t>(1, std::thread::hardware_concurrency() - 1);

    auto app = std::make_shared<VierkantEd>(create_info);
    if(app->parse_override_settings(argc, argv)) { return app->run(); }
    return EXIT_FAILURE;
}