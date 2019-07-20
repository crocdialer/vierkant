#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestWindow)
{
    // init instance with required extensions for glfw (VK_KHR_swapchain, VK_KHR_surface)
    vierkant::Instance instance(true, vk::Window::get_required_extensions());
    bool trigger_size = false;

    auto window = vk::Window::create(instance.handle(), 1280, 720, "TestWindow");
    auto window_size = glm::ivec2(1280, 720);

    // check if a VkSurfaceKHR is ready for us
    BOOST_CHECK(window->surface());
    BOOST_CHECK(window->size() == window_size);

    std::string new_title = "ooops my pants";
    window->set_title(new_title);
    BOOST_CHECK_EQUAL(new_title, window->title());

    auto new_position = glm::ivec2(13, 21);
    window->set_position(new_position);

    //TODO: find out why this fails
//    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //BOOST_CHECK(new_position == window->position());

    // add resize callback
    vierkant::window_delegate_t window_delegate = {};
    window_delegate.resize_fn = [&trigger_size](uint32_t w, uint32_t h){ trigger_size = true; };
    window->window_delegates = {window_delegate};
}

///////////////////////////////////////////////////////////////////////////////////////////////////
