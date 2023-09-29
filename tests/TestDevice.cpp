#include "test_context.hpp"
#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(Device, basic)
{
    vulkan_test_context_t test_context;
    auto device = test_context.device;

    EXPECT_TRUE(device->physical_device());
    EXPECT_TRUE(device->handle() != nullptr);
    EXPECT_TRUE(device->queue(vierkant::Device::Queue::GRAPHICS) != nullptr);
    EXPECT_TRUE(device->queue(vierkant::Device::Queue::TRANSFER) != nullptr);
    EXPECT_TRUE(device->queue(vierkant::Device::Queue::COMPUTE) != nullptr);

    // we didn't order one, so this should be null
    EXPECT_TRUE(device->queue(vierkant::Device::Queue::PRESENT) == nullptr);

    EXPECT_TRUE(device->command_pool_transient());
    EXPECT_TRUE(device->vk_mem_allocator());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
