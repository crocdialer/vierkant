#define BOOST_TEST_MAIN

#include "test_context.hpp"

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestImageFormat)
{
    vierkant::Image::Format fmt = {};
    auto fmt2 = fmt;
    BOOST_CHECK(fmt == fmt2);
    fmt.extent = {1920, 1080, 1};
    BOOST_CHECK(fmt != fmt2);
    std::unordered_map<vierkant::Image::Format, int> fmt_map;
    fmt_map[fmt] = 69;
}

BOOST_AUTO_TEST_CASE(TestImage)
{
    vulkan_test_context_t test_context;

    VkExtent3D size = {1920, 1080, 1};

    // default bytes per pixel (VK_FORMAT_R8G8B8A8_UNORM -> 4)
    size_t bytesPerPixel = vierkant::num_bytes(vierkant::Image::Format().format);
    size_t numBytes = bytesPerPixel * size.width * size.height;
    auto testData = std::unique_ptr<uint8_t[]>(new uint8_t[numBytes]);
    std::fill(&testData[0], &testData[0] + numBytes, 23);

    // only alloc, no upload
    {
        // image for sampling
        vierkant::Image::Format fmt;
        fmt.extent = size;
        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        auto img_sampler = vierkant::Image::create(test_context.device, fmt);

        // image for use as framebuffer-attachment
        fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        fmt.use_mipmap = false;
        auto img_attachment = vierkant::Image::create(test_context.device, fmt);

        // image for sampling with prior mipmap generation
        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        fmt.use_mipmap = true;

        auto img_sampler_mip = vierkant::Image::create(test_context.device, fmt);
        auto buf = vierkant::Buffer::create(test_context.device, testData.get(), numBytes,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);
        vierkant::CommandBuffer cmdBuf(test_context.device, test_context.device->command_pool_transient());
        cmdBuf.begin();

        // copy new data -> will also generate mipmaps
        img_sampler_mip->copy_from(buf, cmdBuf.handle());
        cmdBuf.submit(test_context.device->queue(), true);

    }

    // alloc + upload
    {
        // image for sampling
        vierkant::Image::Format fmt;
        fmt.extent = size;
        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        auto img = vierkant::Image::create(test_context.device, testData.get(), fmt);

        // image for sampling with prior mipmap generation
        fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        fmt.use_mipmap = true;
        auto img_mip = vierkant::Image::create(test_context.device, testData.get(), fmt);

        // create host-visible buffer
        auto hostBuf = vierkant::Buffer::create(test_context.device, nullptr, numBytes,
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VMA_MEMORY_USAGE_CPU_ONLY);
        // download data
//            img_sampler->transitionLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        img->copy_to(hostBuf);

        // check data-integrity
        BOOST_CHECK_EQUAL(memcmp(hostBuf->map(), testData.get(), numBytes), 0);

        // transition back to shader readable layout
        img->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

        // use external CommandBuffer to group commands
        vierkant::CommandBuffer cmdBuf(test_context.device, test_context.device->command_pool_transient());
        cmdBuf.begin();
        img->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmdBuf.handle());
        img_mip->copy_to(hostBuf, cmdBuf.handle());
        img->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmdBuf.handle());

        // submit CommandBuffer, create and wait VkFence internally
        cmdBuf.submit(test_context.device->queue(vierkant::Device::Queue::GRAPHICS), true);

        // check data-integrity
        BOOST_CHECK_EQUAL(memcmp(hostBuf->map(), testData.get(), numBytes), 0);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
