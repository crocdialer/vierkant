#pragma once

#include <boost/test/unit_test.hpp>

#include <vierkant/Device.hpp>

class vulkan_test_context_t
{
public:
    vierkant::Instance instance{};

    vierkant::DevicePtr device = nullptr;

    struct ValidationData
    {
        size_t num_errors = 0;
        std::ostringstream errorMsgStream;

        void reset()
        {
            num_errors = 0;
            errorMsgStream = std::ostringstream{};
        }
    };

    ValidationData validation_data;

    VkSurfaceKHR surface = VK_NULL_HANDLE;


    explicit vulkan_test_context_t(const std::vector<const char *> &extensions = {})
    {
        constexpr bool use_validation = true;
        instance = vierkant::Instance(use_validation, extensions);

        // set a debug-function to intercept validation-warnings/errors
        instance.set_debug_fn([&](const char *msg, VkDebugReportFlagsEXT flags)
                              {
                                  validation_data.num_errors++;
                                  validation_data.errorMsgStream << "\nError:\n" << msg << "\n";
                              });

        BOOST_CHECK_NE(instance.handle(), nullptr);
        BOOST_CHECK_EQUAL(instance.use_validation_layers(), use_validation);
        BOOST_CHECK(!instance.physical_devices().empty());

        // use first device-index
        auto physical_device = instance.physical_devices()[0];

        vierkant::Device::create_info_t deviceInfo = {};
        deviceInfo.instance = instance.handle();
        deviceInfo.physical_device = physical_device;
        deviceInfo.use_validation = instance.use_validation_layers();
        deviceInfo.surface = surface;
        device = vierkant::Device::create(deviceInfo);
    }

    ~vulkan_test_context_t()
    {
        if(validation_data.num_errors){ std::cerr << validation_data.errorMsgStream.str(); }
        BOOST_CHECK(!validation_data.num_errors);
    }
};

