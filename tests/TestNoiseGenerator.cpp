#include "test_context.hpp"
#include "vierkant/NoiseGenerator.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, create)
{
    vulkan_test_context_t test_context;

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = {256, 256, 1};
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);
    EXPECT_NE(generator, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, generate)
{
    vulkan_test_context_t test_context;

    constexpr VkExtent3D size = {256, 256, 1};
    constexpr VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = size;
    create_info.color_format = color_format;
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);
    ASSERT_NE(generator, nullptr);

    auto command_pool = vierkant::create_command_pool(test_context.device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    vierkant::CommandBuffer cmd_buffer(test_context.device, command_pool.get());
    cmd_buffer.begin();
    auto result = generator->generate(cmd_buffer.handle(), {1.f, 1.f}, 0.f);
    cmd_buffer.submit(test_context.device->queue(), true);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->width(), size.width);
    EXPECT_EQ(result->height(), size.height);
    EXPECT_EQ(result->format().format, color_format);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, generate_repeated)
{
    vulkan_test_context_t test_context;

    constexpr VkExtent3D size = {128, 128, 1};

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = size;
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);
    ASSERT_NE(generator, nullptr);

    auto command_pool = vierkant::create_command_pool(test_context.device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    constexpr float scale_step = 0.5f;
    constexpr float seed_step = 0.1f;
    constexpr int num_calls = 5;

    for(int i = 0; i < num_calls; ++i)
    {
        float s = static_cast<float>(i + 1) * scale_step;
        vierkant::CommandBuffer cmd_buffer(test_context.device, command_pool.get());
        cmd_buffer.begin();
        auto result = generator->generate(cmd_buffer.handle(), {s, s}, static_cast<float>(i) * seed_step);
        cmd_buffer.submit(test_context.device->queue(), true);
        EXPECT_NE(result, nullptr);
    }
}
