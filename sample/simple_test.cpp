#include <crocore/filesystem.hpp>
#include <crocore/http.hpp>
#include <crocore/Image.hpp>
#include <vierkant/imgui/imgui_util.h>

#include "simple_test.hpp"

void HelloTriangleApplication::setup()
{
    crocore::g_logger.set_severity(crocore::Severity::DEBUG);

    create_context_and_window();
    create_texture_image();
    load_model();
    create_graphics_pipeline();
}

void HelloTriangleApplication::teardown()
{
    LOG_INFO << "ciao " << name();
    vkDeviceWaitIdle(m_device->handle());
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
    };
    m_window->window_delegates["main"] = window_delegate;

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

    // create a draw context
    m_draw_context = vierkant::DrawContext(m_device);

    // create a gui and add a draw-delegate
    m_gui_context = vk::gui::Context(m_device, g_font_path, 23.f);
    m_gui_context.delegates["application"] = [this]
    {
        vk::gui::draw_application_ui(std::static_pointer_cast<Application>(shared_from_this()));
    };

    // textures window
    m_gui_context.delegates["textures"] = [this] { vk::gui::draw_images_ui({m_texture, m_texture_font}); };

    // animations window
    m_gui_context.delegates["animations"] = [this]
    {
        ImGui::Begin("animations");
//        for(auto &animation : {m_animation})
        {
            float duration = m_animation.duration();
            float current_time = m_animation.progress() * duration;

            // animation current time / duration
            if(ImGui::InputFloat("duration", &duration)){ m_animation.set_duration(duration); }
            ImGui::ProgressBar(m_animation.progress(), ImVec2(-1, 0),
                               crocore::format("%.2f/%.2f s", current_time, duration).c_str());
            ImGui::Separator();

        }
        ImGui::End();
    };

    // imgui demo window
    m_gui_context.delegates["demo"] = [] { if(DEMO_GUI){ ImGui::ShowDemoWindow(&DEMO_GUI); }};

    // attach gui input-delegates to window
    m_window->key_delegates["gui"] = m_gui_context.key_delegate();
    m_window->mouse_delegates["gui"] = m_gui_context.mouse_delegate();

    m_animation = crocore::Animation::create(&m_scale, 0.5f, 1.5f, 2.);
    m_animation.set_ease_function(crocore::easing::EaseOutBounce());
    m_animation.set_loop_type(crocore::Animation::LOOP_BACK_FORTH);
    m_animation.start();

    m_animation.set_duration(3.);

    m_font = vk::Font::create(m_device, g_font_path, 64);
}

void HelloTriangleApplication::create_graphics_pipeline()
{
    auto &framebuffers = m_window->swapchain().framebuffers();

    m_renderer = vk::Renderer(m_device, framebuffers);
    m_image_renderer = vk::Renderer(m_device, framebuffers);
    m_gui_renderer = vk::Renderer(m_device, framebuffers);

    // descriptors
    vk::descriptor_t desc_ubo = {}, desc_texture = {};
    desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    desc_ubo.binding = 0;

    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_texture.binding = 1;
    desc_texture.image_samplers = {m_texture};

    m_drawable = {};
    m_drawable.mesh = m_mesh;
    m_drawable.num_indices = m_mesh->num_elements;
    m_drawable.descriptors = {desc_ubo, desc_texture};

    // with the descriptors in place we can derive the set-layout
    m_drawable.descriptor_set_layout = vk::create_descriptor_set_layout(m_device, m_drawable.descriptors);

    m_drawable.pipeline_format.shader_stages = vierkant::create_shader_stages(m_device, vk::ShaderType::UNLIT_TEXTURE);
    m_drawable.pipeline_format.descriptor_set_layouts = {m_drawable.descriptor_set_layout.get()};
    m_drawable.pipeline_format.primitive_topology = m_mesh->topology;
    m_drawable.pipeline_format.binding_descriptions = vierkant::binding_descriptions(m_mesh);
    m_drawable.pipeline_format.attribute_descriptions = vierkant::attribute_descriptions(m_mesh);
    m_drawable.pipeline_format.depth_test = true;
    m_drawable.pipeline_format.depth_write = true;
    m_drawable.pipeline_format.stencil_test = false;
    m_drawable.pipeline_format.blending = false;
}

void HelloTriangleApplication::create_texture_image()
{
    // try to fetch cool image
    auto http_resonse = cc::net::http::get(g_texture_url);

    crocore::ImagePtr img;
    vk::Image::Format fmt;

    // create from downloaded data
    if(!http_resonse.data.empty()){ img = cc::create_image_from_data(http_resonse.data, 4); }
    else
    {
        // create 2x2 black/white checkerboard image
        uint32_t v[4] = {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF};
        img = cc::Image_<uint8_t>::create(reinterpret_cast<uint8_t *>(v), 2, 2, 4);
        fmt.mag_filter = VK_FILTER_NEAREST;
        fmt.format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    fmt.extent = {img->width(), img->height(), 1};
    fmt.use_mipmap = true;
    m_texture = vk::Image::create(m_device, img->data(), fmt);

    if(m_font)
    {
        // render some text into a texture
        m_texture_font = m_font->create_texture(m_device, "Pooop!\nKleines kaka,\ngrosses KAKA ...");
    }
}

void HelloTriangleApplication::load_model()
{
    auto geom = vk::Geometry::Box(glm::vec3(.5f));
    geom->normals.clear();
    m_mesh = vk::create_mesh_from_geometry(m_device, geom);
}

void HelloTriangleApplication::update(double time_delta)
{
    m_animation.update();

    // update matrices for this frame
    m_drawable.matrices.model = glm::rotate(glm::scale(glm::mat4(1), glm::vec3(m_scale)),
                                            (float)application_time() * glm::radians(30.0f),
                                            glm::vec3(0.0f, 1.0f, 0.0f));
    m_drawable.matrices.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -.5f),
                                           glm::vec3(0.0f, 0.0f, 1.0f));
    m_drawable.matrices.projection = glm::perspective(glm::radians(45.0f), m_window->aspect_ratio(), 0.1f, 10.0f);
    m_drawable.matrices.projection[1][1] *= -1;

    // issue top-level draw-command
    m_window->draw();

    set_running(running() && !m_window->should_close());
}

std::vector<VkCommandBuffer> HelloTriangleApplication::draw(const vierkant::WindowPtr &w)
{
    auto image_index = w->swapchain().image_index();
    const auto &framebuffer = m_window->swapchain().framebuffers()[image_index];
    int32_t width = m_window->swapchain().extent().width, height = m_window->swapchain().extent().height;

    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = framebuffer.handle();
    inheritance.renderPass = framebuffer.renderpass().get();

    for(auto renderer : {&m_image_renderer, &m_renderer, &m_gui_renderer})
    {
        renderer->viewport.width = width;
        renderer->viewport.height = height;
    }

    auto render_images = [this, width, height, &inheritance]() -> VkCommandBuffer
    {
        m_draw_context.draw_image(m_image_renderer, m_texture);
        m_draw_context.draw_image(m_image_renderer, m_texture, {width / 4, height / 4, width / 2, height / 2});
        m_draw_context.draw_image(m_image_renderer, m_texture_font, {width / 4, height / 4, width / 2, height / 2});
        return m_image_renderer.render(&inheritance);
    };

    auto render_mesh = [this, &inheritance]() -> VkCommandBuffer
    {
        m_renderer.stage_drawable(m_drawable);
        return m_renderer.render(&inheritance);
    };

    auto render_gui = [this, &inheritance]() -> VkCommandBuffer
    {
        m_gui_context.render(m_gui_renderer);
        m_draw_context.draw_text(m_gui_renderer, "$$$ oder fahrkarte du nase\nteil zwo", m_font, {400.f, 450.f});
        return m_gui_renderer.render(&inheritance);
    };

    // submit and wait for all command-creation tasks to complete
    std::vector<std::future<VkCommandBuffer>> cmd_futures;
    cmd_futures.push_back(background_queue().post(render_images));
    cmd_futures.push_back(background_queue().post(render_mesh));
    cmd_futures.push_back(background_queue().post(render_gui));
    crocore::wait_all(cmd_futures);

    // get values from completed futures
    std::vector<VkCommandBuffer> command_buffers;
    for(auto &f : cmd_futures){ command_buffers.push_back(f.get()); }
    return command_buffers;
}
