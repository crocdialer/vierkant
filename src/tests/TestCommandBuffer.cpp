#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestCommandBuffer_Constructor)
{
    auto cmdBuf = vk::CommandBuffer();

    // testing operator bool()
    BOOST_CHECK(!cmdBuf);

    // testing default state
    BOOST_CHECK_EQUAL(cmdBuf.is_recording(), false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestCommandBuffer_Submission)
{
    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    BOOST_CHECK(instance);
    BOOST_CHECK(instance.use_validation_layers() == use_validation);
    BOOST_CHECK(!instance.physical_devices().empty());

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);

        std::map<VkCommandPool, VkQueue> poolQueueMap =
                {
                        {device->command_pool(),           device->graphics_queue()},
                        {device->command_pool_transient(), device->graphics_queue()},
                        {device->command_pool_transfer(),  device->transfer_queue()}
                };

        // create command buffers, sourced from different pools
        for(const auto &p : poolQueueMap)
        {
            auto cmdBuf = vk::CommandBuffer(device, p.first);

            // testing operator bool()
            BOOST_CHECK(cmdBuf);
            BOOST_CHECK(!cmdBuf.is_recording());

            cmdBuf.begin(/*VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT*/);
            BOOST_CHECK(cmdBuf.is_recording());
            cmdBuf.end();

            BOOST_CHECK(!cmdBuf.is_recording());

            // submit, do not wait on semaphore, create fence and wait for it
            cmdBuf.submit(p.second, true);

            // reset command buffer
            cmdBuf.reset();

            cmdBuf.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
            BOOST_CHECK(cmdBuf.is_recording());
            cmdBuf.end();

            // submit, do not wait on semaphore, do not wait for completion
            cmdBuf.submit(p.second, true);
        }

        // wait for work to finish on all queues
        vkDeviceWaitIdle(device->handle());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
