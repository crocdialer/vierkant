//
// Created by crocdialer on 9/1/18.
//

#pragma once

#include <iostream>
#include <chrono>
#include <set>
#include <unordered_map>

#include <vulkan/vulkan.h>

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

class HelloTriangleApplication
{

public:

    HelloTriangleApplication();

    void run();

private:

    void init();

    void update(float time_delta);

    void draw();

    void main_loop();

    void create_context_and_window();

    void create_graphics_pipeline();

    void create_command_buffers();

    void create_uniform_buffer();

    void create_texture_image();

    void load_model();

    bool m_use_msaa = true;

    bool m_fullscreen = false;

    // start time
    std::chrono::high_resolution_clock::time_point m_start_time;

    // bundles basic Vulkan assets
    vierkant::Instance m_instance;

    //
    vierkant::DevicePtr m_device;

    // window handle
    vierkant::WindowPtr m_window;

    // the actual pipeline
    vk::Pipeline m_pipeline;

    // command buffers
    std::vector<vierkant::CommandBuffer> m_command_buffers;

    // descriptor pool to allocate sets
    vk::DescriptorPoolPtr m_descriptor_pool;

    vierkant::ImagePtr m_texture;

    vk::MeshPtr m_mesh = vk::Mesh::create();

    std::vector<vierkant::BufferPtr> m_uniform_buffers;
};

int main()
{
    HelloTriangleApplication app;

    try { app.run(); }
    catch(const std::runtime_error &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
