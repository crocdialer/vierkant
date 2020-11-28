#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestImageFormat)
{
    vk::Image::Format fmt = {};
    auto fmt2 = fmt;
    BOOST_CHECK(fmt == fmt2);
    fmt.extent = {1920, 1080, 1};
    BOOST_CHECK(fmt != fmt2);
    std::unordered_map<vk::Image::Format, int> fmt_map;
    fmt_map[fmt] = 69;
}

BOOST_AUTO_TEST_CASE(TestImage)
{
    VkExtent3D size = {1920, 1080, 1};

    // default bytes per pixel (VK_FORMAT_R8G8B8A8_UNORM -> 4)
    size_t bytesPerPixel = vk::num_bytes_per_pixel(vk::Image::Format().format);
    size_t numBytes = bytesPerPixel * size.width * size.height;
    auto testData = std::unique_ptr<uint8_t[]>(new uint8_t[numBytes]);
    std::fill(&testData[0], &testData[0] + numBytes, 23);

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    BOOST_CHECK(instance);
    BOOST_CHECK(instance.use_validation_layers() == use_validation);
    BOOST_CHECK(!instance.physical_devices().empty());

    for(auto physical_device : instance.physical_devices())
    {
        vierkant::Device::create_info_t device_info = {};
        device_info.instance = instance.handle();
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        // only alloc, no upload
        {
            // image for sampling
            vk::Image::Format fmt;
            fmt.extent = size;
            fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            auto img_sampler = vk::Image::create(device, fmt);

            // image for use as framebuffer-attachment
            fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            fmt.use_mipmap = false;
            auto img_attachment = vk::Image::create(device, fmt);

            // image for sampling with prior mipmap generation
            fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            fmt.use_mipmap = true;

            auto img_sampler_mip = vk::Image::create(device, fmt);
            auto buf = vk::Buffer::create(device, testData.get(), numBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VMA_MEMORY_USAGE_CPU_ONLY);
            vk::CommandBuffer cmdBuf(device, device->command_pool_transient());
            cmdBuf.begin();

            // copy new data -> will also generate mipmaps
            img_sampler_mip->copy_from(buf, cmdBuf.handle());
            cmdBuf.submit(device->queue(), true);

        }

        // alloc + upload
        {
            // image for sampling
            vk::Image::Format fmt;
            fmt.extent = size;
            fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            auto img = vk::Image::create(device, testData.get(), fmt);

            // image for sampling with prior mipmap generation
            fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            fmt.use_mipmap = true;
            auto img_mip = vk::Image::create(device, testData.get(), fmt);

            // create host-visible buffer
            auto hostBuf = vk::Buffer::create(device, nullptr, numBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              VMA_MEMORY_USAGE_CPU_ONLY);
            // download data
//            img_sampler->transitionLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            img->copy_to(hostBuf);

            // check data-integrity
            BOOST_CHECK_EQUAL(memcmp(hostBuf->map(), testData.get(), numBytes), 0);

            // transition back to shader readable layout
            img->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // use external CommandBuffer to group commands
            vk::CommandBuffer cmdBuf(device, device->command_pool_transient());
            cmdBuf.begin();
            img->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmdBuf.handle());
            img_mip->copy_to(hostBuf, cmdBuf.handle());
            img->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmdBuf.handle());

            // submit CommandBuffer, create and wait VkFence internally
            cmdBuf.submit(device->queue(vk::Device::Queue::GRAPHICS), true);

            // check data-integrity
            BOOST_CHECK_EQUAL(memcmp(hostBuf->map(), testData.get(), numBytes), 0);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
