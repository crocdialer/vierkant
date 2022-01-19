#define BOOST_TEST_MAIN

#include "test_context.hpp"

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestDevice)
{
    vulkan_test_context_t test_context;
    auto device = test_context.device;

    BOOST_CHECK(device->physical_device());
    BOOST_CHECK(device->handle() != nullptr);
    BOOST_CHECK(device->queue(vk::Device::Queue::GRAPHICS) != nullptr);
    BOOST_CHECK(device->queue(vk::Device::Queue::TRANSFER) != nullptr);
    BOOST_CHECK(device->queue(vk::Device::Queue::COMPUTE) != nullptr);

    // we didn't order one, so this should be null
    BOOST_CHECK(device->queue(vk::Device::Queue::PRESENT) == nullptr);

    BOOST_CHECK(device->command_pool() != nullptr);
    BOOST_CHECK(device->command_pool_transient() != nullptr);
    BOOST_CHECK(device->vk_mem_allocator() != nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
