//
// Created by crocdialer on 9/1/18.
//

#pragma once

#include <crocore/Animation.hpp>
#include "vierkant/vierkant.hpp"
#include "../src/Font.hpp"

const int WIDTH = 1280;
const int HEIGHT = 720;
const bool V_SYNC = true;

////////////////////////////// VALIDATION LAYER ///////////////////////////////////////////////////

#ifdef NDEBUG
const bool g_enable_validation_layers = false;
#else
const bool g_enable_validation_layers = true;
#endif

const char* g_texture_url = "http://roa.h-cdn.co/assets/cm/14/47/1024x576/546b32b33240f_-_hasselhoff_kr_pr_nbc-lg.jpg";

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

    void create_command_buffer(size_t i);

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

    vierkant::ImagePtr m_texture, m_texture_font;

    vk::MeshPtr m_mesh = vk::Mesh::create();

    vk::Renderer::drawable_t m_drawable;

    vk::Renderer m_image_renderer, m_renderer, m_gui_renderer;

    float m_scale = 1.f;

    crocore::Animation m_animation;

    vierkant::Font m_font;

    vierkant::gui::Context m_gui_context;
};

int main(int argc, char *argv[])
{
    auto app = std::make_shared<HelloTriangleApplication>(argc, argv);
    return app->run();
}
