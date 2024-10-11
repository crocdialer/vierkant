#pragma once

#include <gtest/gtest.h>
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
    };

    ValidationData validation_data;

    VkSurfaceKHR surface = VK_NULL_HANDLE;


    explicit vulkan_test_context_t(const std::vector<const char *> &device_extensions = {})
    {
        vierkant::Instance::create_info_t instance_info = {};
        instance_info.use_validation_layers = true;
        instance = vierkant::Instance(instance_info);
        EXPECT_NE(instance.handle(), VK_NULL_HANDLE);

        // set a debug-function to intercept validation-warnings/errors
        instance.set_debug_fn([&](VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                  const VkDebugUtilsMessengerCallbackDataEXT *data) {
            validation_data.num_errors++;
            validation_data.errorMsgStream << "\nError:\n" << data->pMessage << "\n";
        });
        EXPECT_EQ(instance.use_validation_layers(), instance_info.use_validation_layers);
        EXPECT_TRUE(!instance.physical_devices().empty());

        // use first device-index
        auto physical_device = instance.physical_devices()[0];

        vierkant::Device::create_info_t device_info = {};
        device_info.instance = instance.handle();
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();

        // limit testing to two queues
        device_info.max_num_queues = 2;
        device_info.surface = surface;
        device_info.extensions = device_extensions;
        device = vierkant::Device::create(device_info);
    }

    ~vulkan_test_context_t()
    {
        if(validation_data.num_errors) { std::cerr << validation_data.errorMsgStream.str(); }
        EXPECT_TRUE(!validation_data.num_errors);
    }
};
