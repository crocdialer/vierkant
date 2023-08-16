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

    explicit HelloTriangleApplication(const crocore::Application::create_info_t &create_info) : crocore::Application(create_info){};

private:

    void setup() override;

    void update(double time_delta) override;

    void teardown() override;

    void poll_events() override;

    vierkant::window_delegate_t::draw_result_t draw(const vierkant::WindowPtr &w);

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

    vierkant::PerspectiveCameraPtr m_camera;

    vierkant::MeshPtr m_mesh = vierkant::Mesh::create();

    vierkant::drawable_t m_drawable;

    vierkant::Rasterizer m_renderer, m_gui_renderer;

    vierkant::gui::Context m_gui_context;

    std::shared_ptr<entt::registry> m_registry = std::make_shared<entt::registry>();
};

int main(int argc, char *argv[])
{
    crocore::Application::create_info_t create_info = {};
    create_info.arguments = {argv, argv + argc};
    create_info.num_background_threads = 1;
    auto app = std::make_shared<HelloTriangleApplication>(create_info);
    return app->run();
}
