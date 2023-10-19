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
    for(const auto &[pool, queue] : poolQueueMap)
    {
        vierkant::CommandBuffer::create_info_t cmd_buffer_info = {};
        cmd_buffer_info.device = device;
        cmd_buffer_info.command_pool = pool;
        auto cmdBuf = vierkant::CommandBuffer(cmd_buffer_info);

        // testing operator bool()
        EXPECT_TRUE(cmdBuf);
        EXPECT_TRUE(!cmdBuf.is_recording());

        cmdBuf.begin(/*VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT*/);
        EXPECT_TRUE(cmdBuf.is_recording());
        cmdBuf.end();

        EXPECT_TRUE(!cmdBuf.is_recording());

        // submit, do not wait on semaphore, create fence and wait for it
        cmdBuf.submit(queue, true);

        // reset command buffer
        cmdBuf.reset();

        cmdBuf.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
        EXPECT_TRUE(cmdBuf.is_recording());
        cmdBuf.end();

        // submit, do not wait on semaphore, do not wait for completion
        cmdBuf.submit(queue, true);
    }

    // wait for work to finish on all queues
    device->wait_idle();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
