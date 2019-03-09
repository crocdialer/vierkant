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

class HelloTriangleApplication;


////////////////////////////// VALIDATION LAYER ///////////////////////////////////////////////////

#ifdef NDEBUG
const bool enable_validation_layers = false;
#else
const bool g_enable_validation_layers = true;
#endif

////////////////////////////// VERTEX INFORMATION /////////////////////////////////////////////////

struct Vertex
{
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 tex_coord;

    bool operator==(const Vertex &other) const
    {
        return position == other.position && color == other.color && tex_coord == other.tex_coord;
    }
};

namespace std {
template<>
struct hash<Vertex>
{
    size_t operator()(Vertex const &vertex) const
    {
        return ((hash<glm::vec3>()(vertex.position) ^
                 (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.tex_coord) << 1);
    }
};
}

//const std::vector<Vertex> g_vertices =
//        {
//                {{-0.5f, -0.5f, 0.f},   {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
//                {{0.5f,  -0.5f, 0.f},   {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},
//                {{0.5f,  0.5f,  0.f},   {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
//                {{-0.5f, 0.5f,  0.f},   {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}},
//
//                {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
//                {{0.5f,  -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},
//                {{ 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f, 1.f }, { 1.f, 1.f }},
//                {{ -0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.f }, { 0.f, 1.f }},
//        };

const std::vector<glm::vec3> g_vertices =
        {
                {-0.5f, -0.5f, 0.f},
                {0.5f,  -0.5f, 0.f},
                {0.5f,  0.5f,  0.f},
                {-0.5f, 0.5f,  0.f},

                {-0.5f, -0.5f, -0.5f},
                {0.5f,  -0.5f, -0.5f},
                {0.5f,  0.5f,  -0.5f},
                {-0.5f, 0.5f,  -0.5f}
        };

const std::vector<glm::vec2> g_tex_coords =
        {
                {0.f, 0.f},
                {1.f, 0.f},
                {1.f, 1.f},
                {0.f, 1.f},
                {0.f, 0.f},
                {1.f, 0.f},
                {1.f, 1.f},
                {0.f, 1.f}
        };

const std::vector<uint32_t> g_indices =
        {
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7
        };

//const std::string g_model_path = "models/chalet.obj";
//const std::string g_texture_path = "textures/chalet.jpg";
const std::string g_texture_path = "~/Pictures/avatar_01.jpg";

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
