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
    auto fmt = vk::Pipeline::Format();
    fmt.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(m_device, vk::shaders::default_vert);
    fmt.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(m_device, vk::shaders::default_frag);

    fmt.binding_descriptions = vk::binding_descriptions(m_mesh);
    fmt.attribute_descriptions = vk::attribute_descriptions(m_mesh);
    fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//    fmt.front_face = VK_FRONT_FACE_CLOCKWISE;

    fmt.viewport.width = m_window->swapchain().extent().width;
    fmt.viewport.height = m_window->swapchain().extent().height;

    fmt.sample_count = m_window->swapchain().sample_count();
    fmt.depth_test = true;
    fmt.depth_write = true;
    fmt.stencil_test = false;
    fmt.blending = false;
    fmt.descriptor_set_layouts = {m_mesh->descriptor_set_layout.get()};
    fmt.renderpass = m_window->swapchain().renderpass();

    m_pipeline = vk::Pipeline(m_device, fmt);
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

        // bind pipeline
        m_pipeline.bind(m_command_buffers[i].handle());

        VkViewport viewport = {};
        viewport.width = m_window->swapchain().extent().width;
        viewport.height = m_window->swapchain().extent().height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(m_command_buffers[i].handle(), 0, 1, &viewport);

        vk::bind_buffers(m_command_buffers[i].handle(), m_mesh);

        // bind descriptor sets (uniforms, samplers)
        VkDescriptorSet descriptor_set = m_mesh->descriptor_sets[i].get();
        vkCmdBindDescriptorSets(m_command_buffers[i].handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.layout(),
                                0, 1, &descriptor_set, 0, nullptr);

        // issue actual drawing command
        vkCmdDrawIndexed(m_command_buffers[i].handle(), m_mesh->index_buffer->num_bytes() / sizeof(uint32_t), 1, 0, 0,
                         0);
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
    auto geom = vk::Geometry::Plane(1, 1);
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
    desc_texture.image = m_texture;

    m_mesh->descriptors = {desc_ubo, desc_texture};

    // with the descriptors in place we can derive the set-layout
    m_mesh->descriptor_set_layout = vk::create_descriptor_set_layout(m_device, m_mesh);

    // we also need a DescriptorPool ...
    vk::descriptor_count_map_t descriptor_counts;
    vk::add_descriptor_counts(m_mesh, descriptor_counts);
    m_descriptor_pool = vk::create_descriptor_pool(m_device, descriptor_counts);

    // finally create the descriptor-sets for the mesh
    m_mesh->descriptor_sets = vk::create_descriptor_sets(m_device, m_descriptor_pool, m_mesh);
}

void HelloTriangleApplication::update(float time_delta)
{
    auto image_index = m_window->swapchain().image_index();

    // update uniform buffer for this frame
    UniformBuffer ubo = {};
    ubo.model = glm::rotate(glm::mat4(1.0f), time_delta * glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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
