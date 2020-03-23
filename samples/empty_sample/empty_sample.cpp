#include <vierkant/imgui/imgui_util.h>

#include "empty_sample.hpp"

void HelloTriangleApplication::setup()
{
    crocore::g_logger.set_severity(crocore::Severity::DEBUG);

    create_context_and_window();
    load_model();
    create_graphics_pipeline();
}

void HelloTriangleApplication::teardown()
{
    LOG_INFO << "ciao " << name();
    vkDeviceWaitIdle(m_device->handle());
}

void HelloTriangleApplication::poll_events()
{
    glfwPollEvents();
}

void HelloTriangleApplication::create_context_and_window()
{
    m_instance = vk::Instance(g_enable_validation_layers, vk::Window::get_required_extensions());
    m_window = vk::Window::create(m_instance.handle(), WIDTH, HEIGHT, name(), m_fullscreen);
    m_device = vk::Device::create(m_instance.physical_devices().front(), m_instance.use_validation_layers(),
                                  m_window->surface());
    m_window->create_swapchain(m_device, m_use_msaa ? m_device->max_usable_samples() : VK_SAMPLE_COUNT_1_BIT, V_SYNC);

    // create a WindowDelegate
    vierkant::window_delegate_t window_delegate = {};
    window_delegate.draw_fn = std::bind(&HelloTriangleApplication::draw, this, std::placeholders::_1);
    window_delegate.resize_fn = [this](uint32_t w, uint32_t h)
    {
        create_graphics_pipeline();
        m_camera->set_aspect(m_window->aspect_ratio());
    };
    window_delegate.close_fn = [this](){ set_running(false); };
    m_window->window_delegates[name()] = window_delegate;

    // create a KeyDelegate
    vierkant::key_delegate_t key_delegate = {};
    key_delegate.key_press = [this](const vierkant::KeyEvent &e)
    {
        if(!(m_gui_context.capture_flags() & vk::gui::Context::WantCaptureKeyboard))
        {
            if(e.code() == vk::Key::_ESCAPE){ set_running(false); }
        }
    };
    m_window->key_delegates["main"] = key_delegate;

    // create a gui and add a draw-delegate
    m_gui_context = vk::gui::Context(m_device);
    m_gui_context.delegates["application"] = [this]
    {
        vk::gui::draw_application_ui(std::static_pointer_cast<Application>(shared_from_this()), m_window);
    };

    // attach gui input-delegates to window
    m_window->key_delegates["gui"] = m_gui_context.key_delegate();
    m_window->mouse_delegates["gui"] = m_gui_context.mouse_delegate();

    // camera
    m_camera = vk::PerspectiveCamera::create(m_window->aspect_ratio(), 45.f, .1f, 100.f);
    m_camera->set_position(glm::vec3(0.f, 0.f,  2.f));
}

void HelloTriangleApplication::create_graphics_pipeline()
{
    const auto &framebuffers = m_window->swapchain().framebuffers();
    auto fb_extent = framebuffers.front().extent();

    vierkant::Renderer::create_info_t create_info = {};
    create_info.num_frames_in_flight = framebuffers.size();
    create_info.sample_count = m_window->swapchain().sample_count();
    create_info.viewport = {0.f, 0.f, static_cast<float>(fb_extent.width),
                            static_cast<float>(fb_extent.height), 0.f,
                            static_cast<float>(fb_extent.depth)};
    create_info.renderpass = m_window->swapchain().framebuffers().front().renderpass();

    m_renderer = vierkant::Renderer(m_device, create_info);
    m_gui_renderer = vierkant::Renderer(m_device, create_info);
}

void HelloTriangleApplication::load_model()
{
    auto geom = vk::Geometry::create();
    geom->vertices = {glm::vec3(-0.5f, -0.5f, 0.f),
                      glm::vec3(0.5f, -0.5f, 0.f),
                      glm::vec3(0.f, 0.5f, 0.f)};
    geom->colors = {glm::vec4(1.f, 0.f, 0.f, 1.f),
                    glm::vec4(0.f, 1.f, 0.f, 1.f),
                    glm::vec4(0.f, 0.f, 1.f, 1.f)};
    m_mesh = vk::Mesh::create_from_geometries(m_device, {geom});
    m_material->shader_type = vk::ShaderType::UNLIT_COLOR;

    m_drawable = vk::Renderer::create_drawables(m_device, m_mesh, {m_material}).front();
}

void HelloTriangleApplication::update(double time_delta)
{
    m_drawable.matrices.modelview = m_camera->view_matrix();
    m_drawable.matrices.projection = m_camera->projection_matrix();

    // issue top-level draw-command
    m_window->draw();
}

std::vector<VkCommandBuffer> HelloTriangleApplication::draw(const vierkant::WindowPtr &w)
{
    auto image_index = w->swapchain().image_index();
    const auto &framebuffer = m_window->swapchain().framebuffers()[image_index];

    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = framebuffer.handle();
    inheritance.renderPass = framebuffer.renderpass().get();

    auto render_mesh = [this, &inheritance]() -> VkCommandBuffer
    {
        m_renderer.stage_drawable(m_drawable);
        return m_renderer.render(&inheritance);
    };

    auto render_gui = [this, &inheritance]() -> VkCommandBuffer
    {
        m_gui_context.draw_gui(m_gui_renderer);
        return m_gui_renderer.render(&inheritance);
    };

    bool concurrant_draw = true;

    if(concurrant_draw)
    {
        // submit and wait for all command-creation tasks to complete
        std::vector<std::future<VkCommandBuffer>> cmd_futures;
        cmd_futures.push_back(background_queue().post(render_mesh));
        cmd_futures.push_back(background_queue().post(render_gui));
        crocore::wait_all(cmd_futures);

        // get values from completed futures
        std::vector<VkCommandBuffer> command_buffers;
        for(auto &f : cmd_futures){ command_buffers.push_back(f.get()); }
        return command_buffers;
    }
    return {render_mesh(), render_gui()};
}
