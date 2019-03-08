#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "vierkant/vierkant.hpp"

void test_helper(vk::WindowPtr window, VkSampleCountFlagBits sampleCount)
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
    vk::SwapChain swapchain;
    BOOST_CHECK(!swapchain);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestSwapChain_Creation)
{
    // init instance with required extensions for SwapChain (VK_KHR_swapchain, VkSurfaceKHR)
    vierkant::Instance instance(true, vk::Window::get_required_extensions());

    auto window = vk::Window::create(instance.handle(), 1280, 720, "TestSwapchain");

    std::vector<vierkant::DevicePtr> devices;

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         window->surface(), {});

        auto sample_count = VK_SAMPLE_COUNT_1_BIT;
        window->create_swapchain(device);

        BOOST_CHECK(window->swapchain());
        BOOST_CHECK_EQUAL(window->framebuffer_size().x, window->swapchain().extent().width);
        BOOST_CHECK_EQUAL(window->framebuffer_size().y, window->swapchain().extent().height);
        BOOST_CHECK_EQUAL(window->swapchain().sample_count(), sample_count);

        test_helper(window, sample_count);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestSwapChain_Creation_MSAA)
{
    // init instance with required extensions for SwapChain (VK_KHR_swapchain, VkSurfaceKHR)
    vierkant::Instance instance(true, vk::Window::get_required_extensions());

    auto window = vk::Window::create(instance.handle(), 1280, 720, "TestSwapchain MSAA");

    std::vector<vierkant::DevicePtr> devices;

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         window->surface(), {});
        // request maximum MSAA
        auto sample_count = device->max_usable_samples();
        window->create_swapchain(device, sample_count);

        BOOST_CHECK(window->swapchain());
        BOOST_CHECK_EQUAL(window->framebuffer_size().x, window->swapchain().extent().width);
        BOOST_CHECK_EQUAL(window->framebuffer_size().y, window->swapchain().extent().height);
        BOOST_CHECK_EQUAL(window->swapchain().sample_count(), sample_count);

        test_helper(window, sample_count);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////