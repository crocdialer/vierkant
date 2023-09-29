#include <gtest/gtest.h>
#include "vierkant/vierkant.hpp"

const auto window_size = glm::ivec2(1280, 720);

void test_helper(vierkant::WindowPtr window, VkSampleCountFlagBits sampleCount)
{
    EXPECT_TRUE(window->swapchain());
    EXPECT_EQ(window->framebuffer_size().x, window->swapchain().extent().width);
    EXPECT_EQ(window->framebuffer_size().y, window->swapchain().extent().height);
    EXPECT_EQ(window->swapchain().sample_count(), sampleCount);

    // draw one frame
    window->draw();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(SwapChain, Constructor)
{
    vierkant::SwapChain swapchain;
    EXPECT_TRUE(!swapchain);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(SwapChain, Creation)
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

    EXPECT_TRUE(window->swapchain());
    EXPECT_EQ(window->framebuffer_size().x, window->swapchain().extent().width);
    EXPECT_EQ(window->framebuffer_size().y, window->swapchain().extent().height);
    EXPECT_EQ(window->swapchain().sample_count(), sample_count);

    test_helper(window, sample_count);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(SwapChain, Creation_MSAA)
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

    EXPECT_TRUE(window->swapchain());
    EXPECT_EQ(window->framebuffer_size().x, window->swapchain().extent().width);
    EXPECT_EQ(window->framebuffer_size().y, window->swapchain().extent().height);
    EXPECT_EQ(window->swapchain().sample_count(), sample_count);

    test_helper(window, sample_count);
}

///////////////////////////////////////////////////////////////////////////////////////////////////