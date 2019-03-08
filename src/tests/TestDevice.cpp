#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestDevice)
{
    vierkant::Instance instance;
    {
        // vk namespace is just an alias for vierkant
        vk::Instance tmp_instance(true, {});

        // transfer ownership
        instance = std::move(tmp_instance);

        // check that tmp_instance is back in a default, uninitialized state
        BOOST_CHECK(!tmp_instance);
    }

    // those do the same thing, check for a valid handle
    BOOST_CHECK(instance.handle() != nullptr);
    BOOST_CHECK(instance);

    BOOST_CHECK(instance.use_validation_layers());

    std::vector<vierkant::DevicePtr> devices;

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE, {});

        BOOST_CHECK(device->physical_device() == physical_device);
        BOOST_CHECK(device->handle() != nullptr);
        BOOST_CHECK(device->graphics_queue() != nullptr);
        BOOST_CHECK(device->transfer_queue() != nullptr);
        BOOST_CHECK(device->compute_queue() != nullptr);

        // we didn't order one, so this should be null
        BOOST_CHECK(device->present_queue() == nullptr);

        BOOST_CHECK(device->command_pool() != nullptr);
        BOOST_CHECK(device->command_pool_transient() != nullptr);
        BOOST_CHECK(device->vk_mem_allocator() != nullptr);

        devices.push_back(device);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
