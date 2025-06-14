#include <vierkant/imgui/imgui_util.h>

#include "empty_sample.hpp"

void HelloTriangleApplication::setup()
{
    create_context_and_window();
    load_model();
    create_graphics_pipeline();
}

void HelloTriangleApplication::teardown()
{
    spdlog::info("ciao {}", name());
    m_device->wait_idle();
}

void HelloTriangleApplication::poll_events() { m_window->poll_events(); }

void HelloTriangleApplication::create_context_and_window()
{
    vierkant::Instance::create_info_t instance_info = {};
    instance_info.extensions = vierkant::Window::required_extensions();
    instance_info.use_validation_layers = g_enable_validation_layers;
    instance_info.use_debug_labels = g_enable_validation_layers;
    m_instance = vierkant::Instance(instance_info);

    vierkant::Window::create_info_t window_info = {};
    window_info.instance = m_instance.handle();
    window_info.size = {WIDTH, HEIGHT};
    window_info.title = name();
    window_info.fullscreen = m_fullscreen;
    m_window = vierkant::Window::create(window_info);

    // create device
    vierkant::Device::create_info_t device_info = {};
    device_info.instance = m_instance.handle();
    device_info.physical_device = m_instance.physical_devices().front();
    device_info.use_validation = m_instance.use_validation_layers();
    device_info.surface = m_window->surface();
    m_device = vierkant::Device::create(device_info);

    m_window->create_swapchain(m_device, m_use_msaa ? m_device->max_usable_samples() : VK_SAMPLE_COUNT_1_BIT, V_SYNC);

    // create a WindowDelegate
    vierkant::window_delegate_t window_delegate = {};
    window_delegate.draw_fn = [this](const vierkant::WindowPtr &w) { return draw(w); };
    window_delegate.resize_fn = [this](uint32_t w, uint32_t h) {
        create_graphics_pipeline();
        m_camera->perspective_params.aspect = m_window->aspect_ratio();
    };
    window_delegate.close_fn = [this]() { running = false; };
    m_window->window_delegates[name()] = window_delegate;

    // create a KeyDelegate
    vierkant::key_delegate_t key_delegate = {};
    key_delegate.key_press = [this](const vierkant::KeyEvent &e) {
        if(!(m_gui_context.capture_flags() & vierkant::gui::Context::WantCaptureKeyboard))
        {
            if(e.code() == vierkant::Key::_ESCAPE) { running = false; }
        }
    };
    m_window->key_delegates["main"] = key_delegate;

    // create a gui and add a draw-delegate
    vierkant::gui::Context::create_info_t gui_create_info = {};
    gui_create_info.ui_scale = 2.f;
    m_gui_context = vierkant::gui::Context(m_device, gui_create_info);
    m_gui_context.delegates["application"].fn = [this] {
        vierkant::gui::draw_application_ui(std::static_pointer_cast<Application>(shared_from_this()), m_window);
    };

    // attach gui input-delegates to window
    m_window->key_delegates["gui"] = m_gui_context.key_delegate();
    m_window->mouse_delegates["gui"] = m_gui_context.mouse_delegate();

    // camera
    m_camera = vierkant::PerspectiveCamera::create(m_registry);
    m_camera->transform.translation = {0.f, 0.f, 3.f};
}

void HelloTriangleApplication::create_graphics_pipeline()
{
    const auto &framebuffers = m_window->swapchain().framebuffers();
    auto fb_extent = framebuffers.front().extent();

    vierkant::Rasterizer::create_info_t create_info = {};
    create_info.num_frames_in_flight = static_cast<uint32_t>(framebuffers.size());
    create_info.sample_count = m_window->swapchain().sample_count();
    create_info.viewport = {0.f,
                            0.f,
                            static_cast<float>(fb_extent.width),
                            static_cast<float>(fb_extent.height),
                            0.f,
                            static_cast<float>(fb_extent.depth)};

    m_renderer = vierkant::Rasterizer(m_device, create_info);
    m_gui_renderer = vierkant::Rasterizer(m_device, create_info);
}

void HelloTriangleApplication::load_model()
{
    auto geom = vierkant::Geometry::create();
    geom->positions = {glm::vec3(-0.5f, -0.5f, 0.f), glm::vec3(0.5f, -0.5f, 0.f), glm::vec3(0.f, 0.5f, 0.f)};
    geom->colors = {glm::vec4(1.f, 0.f, 0.f, 1.f), glm::vec4(0.f, 1.f, 0.f, 1.f), glm::vec4(0.f, 0.f, 1.f, 1.f)};
    vierkant::Mesh::create_info_t mesh_create_info = {};
    mesh_create_info.mesh_buffer_params.use_vertex_colors = true;
    m_mesh = vierkant::Mesh::create_from_geometry(m_device, geom, mesh_create_info);

    vierkant::create_drawables_params_t drawable_params = {};

    m_drawable = vierkant::create_drawables({m_mesh}, drawable_params).front();
    m_drawable.pipeline_format.shader_stages =
            vierkant::create_shader_stages(m_device, vierkant::ShaderType::UNLIT_COLOR);
}

void HelloTriangleApplication::update(double time_delta)
{
    m_gui_context.update(time_delta, m_window->size());

    m_drawable.matrices.transform = m_camera->view_transform();
    m_drawable.matrices.projection = m_camera->projection_matrix();

    // issue top-level draw-command
    m_window->draw();
}

vierkant::window_delegate_t::draw_result_t HelloTriangleApplication::draw(const vierkant::WindowPtr &w)
{
    const auto &framebuffer = m_window->swapchain().current_framebuffer();

    auto render_mesh = [this, &framebuffer]() -> VkCommandBuffer {
        m_renderer.stage_drawable(m_drawable);
        return m_renderer.render(framebuffer);
    };

    auto render_gui = [this, &framebuffer]() -> VkCommandBuffer {
        m_gui_context.draw_gui(m_gui_renderer);
        return m_gui_renderer.render(framebuffer);
    };

    bool concurrant_draw = true;

    vierkant::window_delegate_t::draw_result_t ret;

    if(concurrant_draw)
    {
        // submit and wait for all command-creation tasks to complete
        std::vector<std::future<VkCommandBuffer>> cmd_futures;
        cmd_futures.push_back(background_queue().post(render_mesh));
        cmd_futures.push_back(background_queue().post(render_gui));
        crocore::wait_all(cmd_futures);

        // get values from completed futures
        for(auto &f: cmd_futures) { ret.command_buffers.push_back(f.get()); }
    }
    else { ret.command_buffers = {render_mesh(), render_gui()}; }
    return ret;
}
