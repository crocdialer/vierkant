#include <gtest/gtest.h>
#include <ranges>
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
TEST(SwapChain, Creation_ColorMode)
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

    using ColorMode = vierkant::SwapChain::ColorMode;
    auto sample_count = VK_SAMPLE_COUNT_1_BIT;

    for(auto mode: {ColorMode::SDR, ColorMode::SDR10, ColorMode::HDR10})
    {
        window->create_swapchain(device, sample_count, true, mode);
        const auto &swapchain = window->swapchain();
        const auto &supported = swapchain.supported_color_modes();
        ASSERT_FALSE(supported.empty());
        EXPECT_EQ(supported.front(), ColorMode::SDR);

        // unsupported modes fall back to the best supported mode below
        auto expected =
                *std::ranges::find_if(supported | std::views::reverse, [mode](ColorMode m) { return m <= mode; });
        EXPECT_EQ(swapchain.color_mode(), expected);
        EXPECT_EQ(swapchain.hdr(), expected == ColorMode::HDR10);

        bool ten_bit = expected != ColorMode::SDR;
        EXPECT_EQ(swapchain.images().front()->format().format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
                          swapchain.images().front()->format().format == VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                  ten_bit);
        EXPECT_EQ(swapchain.color_space(), expected == ColorMode::HDR10 ? VK_COLOR_SPACE_HDR10_ST2084_EXT
                                                                         : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

        // only HDR10 draws ui into a separate layer
        EXPECT_EQ(&window->swapchain().current_framebuffer() != &window->swapchain().framebuffers().front(),
                  expected == ColorMode::HDR10);

        test_helper(window, sample_count);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
