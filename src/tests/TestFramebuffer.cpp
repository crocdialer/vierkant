#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "../../include/vierkant/Framebuffer.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

using attachment_count_t = std::map<vk::Framebuffer::Attachment, uint32_t>;

bool check_attachment_count(const vk::Framebuffer::AttachmentMap &attachments,
                            std::map<vk::Framebuffer::Attachment, uint32_t> &expected_count)
{
    if(attachments.count(vk::Framebuffer::Attachment::Color) !=
       expected_count.count(vk::Framebuffer::Attachment::Color)){ return false; }
    if(attachments.count(vk::Framebuffer::Attachment::Resolve) !=
       expected_count.count(vk::Framebuffer::Attachment::Resolve)){ return false; }
    if(attachments.count(vk::Framebuffer::Attachment::DepthStencil) !=
       expected_count.count(vk::Framebuffer::Attachment::DepthStencil)){ return false; }

    for(auto &pair : attachments)
    {
        if(pair.second.size() != expected_count[pair.first]){ return false; }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestFramebuffer_Constructor)
{
    vk::Framebuffer framebuffer;
    BOOST_CHECK(!framebuffer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColor)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);

        // only 1 color attachment, no depth/stencil
        auto framebuffer = vk::Framebuffer(device, fb_size);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.extent().width, fb_size.width);
        BOOST_CHECK_EQUAL(framebuffer.extent().height, fb_size.height);
        BOOST_CHECK_EQUAL(framebuffer.extent().depth, fb_size.depth);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 1);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::Attachment::Color] = 1;
        auto res = check_attachment_count(framebuffer.attachments(), expected_count);
        BOOST_CHECK(res);

        // record starting a renderpass

        // set clear values
        framebuffer.clear_color = {0.f, 0.f, 0.f, 1.f};
        framebuffer.clear_depth_stencil = {1.f, 0};

        vk::CommandBuffer cmd_buf(device, device->command_pool());
        cmd_buf.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
        framebuffer.begin_renderpass(cmd_buf.handle());
        framebuffer.end_renderpass();
        cmd_buf.end();

        // reset Framebuffer object
        framebuffer = vk::Framebuffer();
        BOOST_CHECK(!framebuffer);
    }
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColorDepth)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);


        // 1 color attachment plus depth
        vk::Framebuffer::Format fmt;
        fmt.depth = true;
        auto framebuffer = vk::Framebuffer(device, fb_size, fmt);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 2);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::Attachment::Color] = 1;
        expected_count[vk::Framebuffer::Attachment::DepthStencil] = 1;
        auto res = check_attachment_count(framebuffer.attachments(), expected_count);
        BOOST_CHECK(res);
    }
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColorDepthStencil)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);

        // 1 color attachment plus depth/stencil
        vk::Framebuffer::Format fmt;
        fmt.depth = true;
        fmt.stencil = true;
        auto framebuffer = vk::Framebuffer(device, fb_size, fmt);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 2);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::Attachment::Color] = 1;
        expected_count[vk::Framebuffer::Attachment::DepthStencil] = 1;
        auto res = check_attachment_count(framebuffer.attachments(), expected_count);
        BOOST_CHECK(res);
    }
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_MultiColorDepthStencil)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);

        // 4 color attachments plus depth/stencil
        vk::Framebuffer::Format fmt;
        fmt.num_color_attachments = 4;
        fmt.depth = true;
        fmt.stencil = true;
        auto framebuffer = vk::Framebuffer(device, fb_size, fmt);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 5);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::Attachment::Color] = 4;
        expected_count[vk::Framebuffer::Attachment::DepthStencil] = 1;
        auto res = check_attachment_count(framebuffer.attachments(), expected_count);
        BOOST_CHECK(res);
    }
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColorDepthStencil_MSAA)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);

        // 1 color attachment (MSAA) | depth/stencil (MSAA) | resolve
        vk::Framebuffer::Format fmt;
        fmt.num_color_attachments = 1;
        fmt.depth = true;
        fmt.stencil = true;
        fmt.color_attachment_format.sample_count = device->max_usable_samples();
        auto framebuffer = vk::Framebuffer(device, fb_size, fmt);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 3);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::Attachment::Color] = 1;
        expected_count[vk::Framebuffer::Attachment::DepthStencil] = 1;
        expected_count[vk::Framebuffer::Attachment::Resolve] = 1;
        auto res = check_attachment_count(framebuffer.attachments(), expected_count);
        BOOST_CHECK(res);
    }
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_Manual_Attachments)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);

        // manually creating attachments
        // 1 color attachment (MSAA) | depth/stencil (MSAA) | resolve

        // color
        vk::Image::Format color_fmt;
        color_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        color_fmt.sample_count = device->max_usable_samples();
        auto color_img = vk::Image::create(device, fb_size, color_fmt);

        // depth/stencil
        vk::Image::Format depth_stencil_fmt;
        depth_stencil_fmt.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_stencil_fmt.sample_count = device->max_usable_samples();
        depth_stencil_fmt.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        depth_stencil_fmt.aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        auto depth_stencil_img = vk::Image::create(device, fb_size, depth_stencil_fmt);

        vk::Image::Format resolve_fmt;
        resolve_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        resolve_fmt.sample_count = VK_SAMPLE_COUNT_1_BIT;
        auto resolve_img = vk::Image::create(device, fb_size, resolve_fmt);

        vk::Framebuffer::AttachmentMap attachments = {
                {vk::Framebuffer::Attachment::Color,        {color_img}},
                {vk::Framebuffer::Attachment::DepthStencil, {depth_stencil_img}},
                {vk::Framebuffer::Attachment::Resolve,      {resolve_img}}
        };

        auto framebuffer = vk::Framebuffer(device, attachments);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 3);
        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::Attachment::Color] = 1;
        expected_count[vk::Framebuffer::Attachment::DepthStencil] = 1;
        expected_count[vk::Framebuffer::Attachment::Resolve] = 1;
        auto res = check_attachment_count(framebuffer.attachments(), expected_count);
        BOOST_CHECK(res);

    }
}