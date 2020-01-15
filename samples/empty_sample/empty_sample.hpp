//
// Created by crocdialer on 9/1/18.
//

#pragma once

#include <crocore/Application.hpp>
#include <crocore/Animation.hpp>

#include <vierkant/vierkant.hpp>

const int WIDTH = 1920;
const int HEIGHT = 1080;
const bool V_SYNC = true;
bool DEMO_GUI = true;

////////////////////////////// VALIDATION LAYER ///////////////////////////////////////////////////

#ifdef NDEBUG
const bool g_enable_validation_layers = false;
#else
const bool g_enable_validation_layers = true;
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

class HelloTriangleApplication : public crocore::Application
{

public:

    explicit HelloTriangleApplication(int argc = 0, char *argv[] = nullptr) : crocore::Application(argc, argv) {};

private:

    void setup() override;

    void update(double time_delta) override;

    void teardown() override;

    void poll_events() override;

    std::vector<VkCommandBuffer> draw(const vierkant::WindowPtr &w);

    void create_context_and_window();

    void create_graphics_pipeline();

    void load_model();

    bool m_use_msaa = true;

    bool m_fullscreen = false;

    // bundles basic Vulkan assets
    vierkant::Instance m_instance;

    // device
    vierkant::DevicePtr m_device;

    // window handle
    std::shared_ptr<vierkant::Window> m_window;

    vk::PerspectiveCameraPtr m_camera;

    vk::MeshPtr m_mesh = vk::Mesh::create();

    vk::MaterialPtr m_material = vk::Material::create();

    vk::Renderer::drawable_t m_drawable;

    vk::Renderer m_renderer, m_gui_renderer;

    vierkant::gui::Context m_gui_context;
};

int main(int argc, char *argv[])
{
    auto app = std::make_shared<HelloTriangleApplication>(argc, argv);
    return app->run();
}
