#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "vierkant/vierkant.hpp"

const auto window_size = glm::ivec2(1280, 720);

void test_helper(vierkant::WindowPtr window, VkSampleCountFlagBits sampleCount)
{
    BOOST_CHECK(window->swapchain());
    BOOST_CHECK_EQUAL(window->framebuffer_size().x, window->swapchain().extent().width);
    BOOST_CHECK_EQUAL(window->framebuffer_size().y, window->swapchain().extent().height);
    BOOST_CHECK_EQUAL(window->swapchain().sample_count(), sampleCount);

    // draw one frame
    window->draw();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestSwapChain_Constructor)
{
    vierkant::SwapChain swapchain;
    BOOST_CHECK(!swapchain);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestSwapChain_Creation)
{
    vierkant::Instance::create_info_t instance_info = {};
    instance_info.extensions = vierkant::Window::required_extensions();
    instance_info.use_validation_layers = true;
    auto instance = vierkant::Instance(instance_info);

    vierkant::Window::create_info_t window_info = {};
    window_info.instance = instance.handle();
    window_info.size = window_size;
    window_info.title = "TestSwapchain";
    window_info.fullscreen = false;
    auto window = vierkant::Window::create(window_info);

    vierkant::Device::create_info_t device_info = {};
    device_info.instance = instance.handle();
    device_info.physical_device = instance.physical_devices().front();
    device_info.use_validation = instance.use_validation_layers();
    device_info.surface = window->surface();
    auto device = vierkant::Device::create(device_info);

    auto sample_count = VK_SAMPLE_COUNT_1_BIT;
    window->create_swapchain(device);

    BOOST_CHECK(window->swapchain());
    BOOST_CHECK_EQUAL(window->framebuffer_size().x, window->swapchain().extent().width);
    BOOST_CHECK_EQUAL(window->framebuffer_size().y, window->swapchain().extent().height);
    BOOST_CHECK_EQUAL(window->swapchain().sample_count(), sample_count);

    test_helper(window, sample_count);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestSwapChain_Creation_MSAA)
{
    vierkant::Instance::create_info_t instance_info = {};
    instance_info.extensions = vierkant::Window::required_extensions();
    instance_info.use_validation_layers = true;
    auto instance = vierkant::Instance(instance_info);

    vierkant::Window::create_info_t window_info = {};
    window_info.instance = instance.handle();
    window_info.size = window_size;
    window_info.title = "TestSwapchain";
    window_info.fullscreen = false;
    auto window = vierkant::Window::create(window_info);

    vierkant::Device::create_info_t device_info = {};
    device_info.instance = instance.handle();
    device_info.physical_device = instance.physical_devices().front();
    device_info.use_validation = instance.use_validation_layers();
    device_info.surface = window->surface();
    auto device = vierkant::Device::create(device_info);

    // request maximum MSAA
    auto sample_count = device->max_usable_samples();
    window->create_swapchain(device, sample_count);

    BOOST_CHECK(window->swapchain());
    BOOST_CHECK_EQUAL(window->framebuffer_size().x, window->swapchain().extent().width);
    BOOST_CHECK_EQUAL(window->framebuffer_size().y, window->swapchain().extent().height);
    BOOST_CHECK_EQUAL(window->swapchain().sample_count(), sample_count);

    test_helper(window, sample_count);
}

///////////////////////////////////////////////////////////////////////////////////////////////////