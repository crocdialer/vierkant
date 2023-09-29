#include "test_context.hpp"
#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestCommandBuffer, Constructor)
{
    auto cmdBuf = vierkant::CommandBuffer();

    // testing operator bool()
    EXPECT_TRUE(!cmdBuf);

    // testing default state
    EXPECT_EQ(cmdBuf.is_recording(), false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestCommandBuffer, Submission)
{
    vulkan_test_context_t test_context;
    auto device = test_context.device;

    std::map<VkCommandPool, VkQueue> poolQueueMap =
            {
                    {device->command_pool_transient(), device->queue(vierkant::Device::Queue::GRAPHICS)},
                    {device->command_pool_transfer(),  device->queue(vierkant::Device::Queue::TRANSFER)}
            };

    // create command buffers, sourced from different pools
    for(const auto &p : poolQueueMap)
    {
        auto cmdBuf = vierkant::CommandBuffer(device, p.first);

        // testing operator bool()
        EXPECT_TRUE(cmdBuf);
        EXPECT_TRUE(!cmdBuf.is_recording());

        cmdBuf.begin(/*VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT*/);
        EXPECT_TRUE(cmdBuf.is_recording());
        cmdBuf.end();

        EXPECT_TRUE(!cmdBuf.is_recording());

        // submit, do not wait on semaphore, create fence and wait for it
        cmdBuf.submit(p.second, true);

        // reset command buffer
        cmdBuf.reset();

        cmdBuf.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
        EXPECT_TRUE(cmdBuf.is_recording());
        cmdBuf.end();

        // submit, do not wait on semaphore, do not wait for completion
        cmdBuf.submit(p.second, true);
    }

    // wait for work to finish on all queues
    device->wait_idle();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
