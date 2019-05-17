//
// Created by crocdialer on 9/1/18.
//

#pragma once

#include "vierkant/vierkant.hpp"

const int WIDTH = 1280;
const int HEIGHT = 720;

////////////////////////////// VALIDATION LAYER ///////////////////////////////////////////////////

#ifdef NDEBUG
const bool g_enable_validation_layers = false;
#else
const bool g_enable_validation_layers = true;
#endif

const std::string g_texture_path = "~/Pictures/renderer_of_worlds.jpg";

////////////////////////////// UNIFORM INFORMATION /////////////////////////////////////////////////

struct UniformBuffer
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class HelloTriangleApplication : public vierkant::Application
{

public:

    explicit HelloTriangleApplication(int argc = 0, char *argv[] = nullptr) : vierkant::Application(argc, argv) {};

private:

    void setup() override;

    void update(double time_delta) override;

    void teardown() override;

    void draw(const vierkant::WindowPtr &w);

    void create_context_and_window();

    void create_graphics_pipeline();

    void create_command_buffers();

    void create_uniform_buffer();

    void create_texture_image();

    void load_model();

    bool m_use_msaa = true;

    bool m_fullscreen = false;

    // bundles basic Vulkan assets
    vierkant::Instance m_instance;

    //
    vierkant::DevicePtr m_device;

    // window handle
    std::shared_ptr<vierkant::Window> m_window;

    // command buffers
    std::vector<vierkant::CommandBuffer> m_command_buffers;

    // descriptor pool to allocate sets
    vk::DescriptorPoolPtr m_descriptor_pool;

    vierkant::ImagePtr m_texture;

    vk::MeshPtr m_mesh = vk::Mesh::create();

    std::vector<vk::Renderer::drawable_t> m_drawables;

    std::vector<vierkant::BufferPtr> m_uniform_buffers;

    vk::Renderer m_renderer;
};

int main(int argc, char *argv[])
{
    auto app = std::make_unique<HelloTriangleApplication>(argc, argv);
    return app->run();
}
