#include <crocore/filesystem.hpp>
#include <crocore/http.hpp>
#include <crocore/Image.hpp>
#include "simple_test.hpp"

void HelloTriangleApplication::setup()
{
    crocore::Logger::get()->set_severity(crocore::Severity::DEBUG);

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

    m_window->draw_fn = std::bind(&HelloTriangleApplication::draw, this, std::placeholders::_1);
    m_window->resize_fn = [this](uint32_t w, uint32_t h)
    {
        create_graphics_pipeline();
    };

    m_window->key_delegate.key_press = [this](const vierkant::KeyEvent &e)
    {
        if(e.code() == vk::Key::_ESCAPE){ set_running(false); }
    };

    m_animation = crocore::Animation::create(&m_scale, 0.5f, 1.5f, 2.f);
    m_animation.set_ease_function(crocore::easing::EaseOutBounce());
    m_animation.set_loop_type(crocore::Animation::LOOP_BACK_FORTH);
    m_animation.start();

//    m_font.load(m_device, "/usr/local/share/fonts/Courier New Bold.ttf", 64);
}

void HelloTriangleApplication::create_graphics_pipeline()
{
    auto &framebuffers = m_window->swapchain().framebuffers();

    m_renderer = vk::Renderer(m_device, framebuffers);

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
    m_drawable.descriptors = {desc_ubo, desc_texture};

    // with the descriptors in place we can derive the set-layout
    m_drawable.descriptor_set_layout = vk::create_descriptor_set_layout(m_device, m_drawable.descriptors);

    m_drawable.pipeline_format.shader_stages = vierkant::shader_stages(m_device, vk::ShaderType::UNLIT_TEXTURE);
    m_drawable.pipeline_format.descriptor_set_layouts = {m_drawable.descriptor_set_layout.get()};
    m_drawable.pipeline_format.primitive_topology = m_mesh->topology;
    m_drawable.pipeline_format.binding_descriptions = vierkant::binding_descriptions(m_mesh);
    m_drawable.pipeline_format.attribute_descriptions = vierkant::attribute_descriptions(m_mesh);
    m_drawable.pipeline_format.depth_test = true;
    m_drawable.pipeline_format.depth_write = true;
    m_drawable.pipeline_format.stencil_test = false;
    m_drawable.pipeline_format.blending = false;

    // make sure count matches
    m_command_buffers.resize(framebuffers.size());

    for(auto &cmd_buf : m_command_buffers)
    {
        cmd_buf = vk::CommandBuffer(m_device, m_device->command_pool(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }
}

void HelloTriangleApplication::create_command_buffer(size_t i)
{
    const auto &framebuffer = m_window->swapchain().framebuffers()[i];
    auto &command_buffer = m_command_buffers[i];

    VkCommandBufferInheritanceInfo inheritance = {};
    inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance.framebuffer = framebuffer.handle();
    inheritance.renderPass = framebuffer.renderpass().get();

    command_buffer.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT |
                         VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance);

    m_renderer.viewport.width = m_window->swapchain().extent().width;
    m_renderer.viewport.height = m_window->swapchain().extent().height;

    m_renderer.draw_image(command_buffer.handle(), m_texture,
                          {0, 0, m_renderer.viewport.width, m_renderer.viewport.height});
    m_renderer.draw(command_buffer.handle(), m_drawable);

    command_buffer.end();
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

//    m_texture = m_font.create_texture(m_device, "Pooop!\nKleines kaka,\ngrosses KAKA ...");
//
//    fmt = m_texture->format();
//    fmt.component_swizzle = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
//                             VK_COMPONENT_SWIZZLE_ONE};
//    m_texture_swizzled = vk::Image::create(m_device, m_texture->shared_image(), fmt);
}

void HelloTriangleApplication::load_model()
{
    auto geom = vk::Geometry::Box(glm::vec3(.5f));
    geom->normals.clear();
//    vk::compute_half_edges(geom);

//    auto geom = vk::Geometry::BoxOutline();
//    auto geom = vk::Geometry::Grid();
    m_mesh = vk::create_mesh_from_geometry(m_device, geom);
}

void HelloTriangleApplication::update(double time_delta)
{
    m_animation.update();

    // update matrices for this frame
    m_drawable.matrices.model = glm::rotate(glm::scale(glm::mat4(1), glm::vec3(m_scale)),
                                            (float)get_application_time() * glm::radians(30.0f),
                                            glm::vec3(0.0f, 1.0f, 0.0f));
    m_drawable.matrices.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -.5f),
                                           glm::vec3(0.0f, 0.0f, 1.0f));
    m_drawable.matrices.projection = glm::perspective(glm::radians(45.0f), m_window->aspect_ratio(), 0.1f, 10.0f);
    m_drawable.matrices.projection[1][1] *= -1;

    // issue top-level draw-command
    m_window->draw();

    set_running(running() && !m_window->should_close());
}

void HelloTriangleApplication::draw(const vierkant::WindowPtr &w)
{
    auto image_index = w->swapchain().image_index();

    // set index for renderer
    m_renderer.set_current_index(image_index);

    // create commandbuffer for this frame
    create_command_buffer(image_index);

    // execute secondary commandbuffers
    VkCommandBuffer command_buf = m_command_buffers[image_index].handle();
    vkCmdExecuteCommands(w->command_buffer().handle(), 1, &command_buf);
}
