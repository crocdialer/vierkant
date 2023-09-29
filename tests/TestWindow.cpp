#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(Window, basic)
{
    vierkant::Instance::create_info_t instance_info = {};
    instance_info.extensions = vierkant::Window::required_extensions();
    instance_info.use_validation_layers = true;
    auto instance = vierkant::Instance(instance_info);

    bool trigger_size = false;

    const auto window_size = glm::ivec2(1280, 720);

    vierkant::Window::create_info_t window_info = {};
    window_info.instance = instance.handle();
    window_info.size = window_size;
    window_info.title = "TestWindow";
    window_info.fullscreen = false;
    auto window = vierkant::Window::create(window_info);

    // check if a VkSurfaceKHR is ready for us
    EXPECT_TRUE(window->surface());
    EXPECT_TRUE(window->size() == window_size);

    std::string new_title = "ooops my pants";
    window->set_title(new_title);
    EXPECT_EQ(new_title, window->title());

    auto new_position = glm::ivec2(13, 21);
    window->set_position(new_position);

//    //TODO: find out why this fails
//    std::this_thread::sleep_for(std::chrono::milliseconds(500));
//    EXPECT_TRUE(new_position == window->position());

    // add resize callback
    vierkant::window_delegate_t window_delegate = {};
    window_delegate.resize_fn = [&trigger_size](uint32_t w, uint32_t h){ trigger_size = true; };
    window->window_delegates["default"] = window_delegate;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
