#include "simple_test.hpp"

#include "crocore/filesystem.hpp"
#include "crocore/Image.hpp"

using float_sec_t = std::chrono::duration<float, std::chrono::seconds::period>;

HelloTriangleApplication::HelloTriangleApplication() :
        m_start_time(std::chrono::high_resolution_clock::now())
{

}

void HelloTriangleApplication::run()
{
    init();
    main_loop();
}

void HelloTriangleApplication::init()
{
    create_context_and_window();
    create_texture_image();
    create_uniform_buffer();
    load_model();
    create_graphics_pipeline();
    create_command_buffers();
}

void HelloTriangleApplication::main_loop()
{
    while(!m_window->should_close())
    {
        glfwPollEvents();

        auto current_time = std::chrono::high_resolution_clock::now();
        float time_delta = float_sec_t(current_time - m_start_time).count();

        update(time_delta);

        m_window->draw();
    }
    vkDeviceWaitIdle(m_device->handle());
}

void HelloTriangleApplication::create_context_and_window()
{
    m_instance = vk::Instance(g_enable_validation_layers, vk::Window::get_required_extensions());
    m_window = vk::Window::create(m_instance.handle(), WIDTH, HEIGHT, "Vulkan", m_fullscreen);
    m_device = vk::Device::create(m_instance.physical_devices().front(), m_instance.use_validation_layers(),
                                  m_window->surface());
    m_window->create_swapchain(m_device, m_use_msaa ? m_device->max_usable_samples() : VK_SAMPLE_COUNT_1_BIT);
    m_window->set_draw_fn(std::bind(&HelloTriangleApplication::draw, this));
    m_window->set_resize_fn([this](uint32_t w, uint32_t h)
                            {
                                create_graphics_pipeline();
                                create_command_buffers();
                            });
}

void HelloTriangleApplication::create_graphics_pipeline()
{
    m_renderer = vk::Renderer(m_device, m_window->swapchain().framebuffers().front());
    m_drawables.resize(m_window->swapchain().framebuffers().size());

    auto descriptor_sets = vk::create_descriptor_sets(m_device, m_descriptor_pool, m_mesh);

    for(uint32_t i = 0; i < m_drawables.size(); ++i)
    {
        m_drawables[i].mesh = m_mesh;
        m_drawables[i].descriptor_set = descriptor_sets[i];
        m_drawables[i].pipeline_format.depth_test = true;
        m_drawables[i].pipeline_format.depth_write = true;
        m_drawables[i].pipeline_format.stencil_test = false;
        m_drawables[i].pipeline_format.blending = false;
    }

}

void HelloTriangleApplication::create_command_buffers()
{
    const auto &framebuffers = m_window->swapchain().framebuffers();

    m_command_buffers.resize(framebuffers.size());

    for(size_t i = 0; i < m_command_buffers.size(); i++)
    {
        m_command_buffers[i] = vk::CommandBuffer(m_device, m_device->command_pool(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        VkCommandBufferInheritanceInfo inheritance = {};
        inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritance.framebuffer = framebuffers[i].handle();
        inheritance.renderPass = framebuffers[i].renderpass().get();

        m_command_buffers[i].begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT |
                                   VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance);

        m_renderer.viewport.width = m_window->swapchain().extent().width;
        m_renderer.viewport.height = m_window->swapchain().extent().height;
        m_renderer.draw(m_command_buffers[i].handle(), m_drawables[i]);

        m_command_buffers[i].end();
    }
}

void HelloTriangleApplication::create_uniform_buffer()
{
    VkDeviceSize buf_size = sizeof(UniformBuffer);
    m_uniform_buffers.resize(m_window->swapchain().images().size());

    for(size_t i = 0; i < m_window->swapchain().images().size(); i++)
    {
        m_uniform_buffers[i] = vk::Buffer::create(m_device, nullptr, buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void HelloTriangleApplication::create_texture_image()
{
    auto img = cc::create_image_from_file(g_texture_path, 4);
    vk::Image::Format fmt;
    fmt.use_mipmap = true;
    m_texture = vk::Image::create(m_device, img->data(), {img->width(), img->height(), 1}, fmt);
}

void HelloTriangleApplication::load_model()
{
//    auto geom = vk::Geometry::Plane(1, 1);
    auto geom = vk::Geometry::Box(glm::vec3(.5f));
//    auto geom = vk::Geometry::BoxOutline();
//    auto geom = vk::Geometry::Grid();
    m_mesh = vk::create_mesh_from_geometry(m_device, geom);

    // descriptors
    vk::Mesh::Descriptor desc_ubo, desc_texture;
    desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_ubo.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    desc_ubo.binding = 0;
    desc_ubo.buffers = m_uniform_buffers;

    desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc_texture.binding = 1;
    desc_texture.image_samplers = {m_texture};

    m_mesh->descriptors = {desc_ubo, desc_texture};

    // with the descriptors in place we can derive the set-layout
    m_mesh->descriptor_set_layout = vk::create_descriptor_set_layout(m_device, m_mesh);

    // we also need a DescriptorPool ...
    vk::descriptor_count_t descriptor_counts;
    vk::add_descriptor_counts(m_mesh, descriptor_counts);
    m_descriptor_pool = vk::create_descriptor_pool(m_device, descriptor_counts,
                                                   m_window->swapchain().framebuffers().size());

    // finally create the descriptor-sets for the mesh
//    m_mesh->descriptor_sets = vk::create_descriptor_sets(m_device, m_descriptor_pool, m_mesh);
}

void HelloTriangleApplication::update(float time_delta)
{
    auto image_index = m_window->swapchain().image_index();

    // update uniform buffer for this frame
    UniformBuffer ubo = {};
    ubo.model = glm::rotate(glm::mat4(1.0f), time_delta * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -.5f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.projection = glm::perspective(glm::radians(45.0f), m_window->aspect_ratio(), 0.1f, 10.0f);
    ubo.projection[1][1] *= -1;
    m_uniform_buffers[image_index]->set_data(&ubo, sizeof(ubo));
}

void HelloTriangleApplication::draw()
{
    auto image_index = m_window->swapchain().image_index();
    std::vector<VkCommandBuffer> command_bufs = {m_command_buffers[image_index].handle()};
    vkCmdExecuteCommands(m_window->command_buffer().handle(), static_cast<uint32_t>(command_bufs.size()),
                         command_bufs.data());
}
