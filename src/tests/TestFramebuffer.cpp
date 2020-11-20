#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

using attachment_count_t = std::map<vk::Framebuffer::AttachmentType, uint32_t>;

bool check_attachment_count(const vk::Framebuffer::AttachmentMap &attachments,
                            std::map<vk::Framebuffer::AttachmentType, uint32_t> &expected_count)
{
    if(attachments.count(vk::Framebuffer::AttachmentType::Color) !=
       expected_count.count(vk::Framebuffer::AttachmentType::Color)){ return false; }
    if(attachments.count(vk::Framebuffer::AttachmentType::Resolve) !=
       expected_count.count(vk::Framebuffer::AttachmentType::Resolve)){ return false; }
    if(attachments.count(vk::Framebuffer::AttachmentType::DepthStencil) !=
       expected_count.count(vk::Framebuffer::AttachmentType::DepthStencil)){ return false; }

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
        vierkant::Device::create_info_t device_info = {};
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        vierkant::Framebuffer::create_info_t create_info = {};
        create_info.size = fb_size;

        // only 1 color attachment, no depth/stencil
        auto framebuffer = vk::Framebuffer(device, create_info);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.extent().width, fb_size.width);
        BOOST_CHECK_EQUAL(framebuffer.extent().height, fb_size.height);
        BOOST_CHECK_EQUAL(framebuffer.extent().depth, fb_size.depth);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 1);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::AttachmentType::Color] = 1;
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
        vierkant::Device::create_info_t device_info = {};
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);


        // 1 color attachment plus depth
        vierkant::Framebuffer::create_info_t create_info = {};
        create_info.size = fb_size;
        create_info.depth = true;
        auto framebuffer = vk::Framebuffer(device, create_info);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::Color), 1);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::DepthStencil), 1);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::AttachmentType::Color] = 1;
        expected_count[vk::Framebuffer::AttachmentType::DepthStencil] = 1;
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
        vierkant::Device::create_info_t device_info = {};
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        // 1 color attachment plus depth/stencil
        vierkant::Framebuffer::create_info_t create_info = {};
        create_info.size = fb_size;
        create_info.depth = true;
        create_info.stencil = true;
        auto framebuffer = vierkant::Framebuffer(device, create_info);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::Color), 1);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::DepthStencil), 1);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::AttachmentType::Color] = 1;
        expected_count[vk::Framebuffer::AttachmentType::DepthStencil] = 1;
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
        vierkant::Device::create_info_t device_info = {};
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        // 4 color attachments plus depth/stencil
        vierkant::Framebuffer::create_info_t create_info = {};
        create_info.size = fb_size;
        create_info.num_color_attachments = 4;
        create_info.depth = true;
        create_info.stencil = true;
        auto framebuffer = vk::Framebuffer(device, create_info);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::Color), 4);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::DepthStencil), 1);

        attachment_count_t expected_count;
        expected_count[vierkant::Framebuffer::AttachmentType::Color] = 4;
        expected_count[vierkant::Framebuffer::AttachmentType::DepthStencil] = 1;
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
        vierkant::Device::create_info_t device_info = {};
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        // 1 color attachment (MSAA) | depth/stencil (MSAA) | resolve
        vierkant::Framebuffer::create_info_t create_info = {};
        create_info.size = fb_size;
        create_info.num_color_attachments = 1;
        create_info.depth = true;
        create_info.stencil = true;
        create_info.color_attachment_format.sample_count = device->max_usable_samples();
        auto framebuffer = vierkant::Framebuffer(device, create_info);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::Color), 1);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::Resolve), 1);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::DepthStencil), 1);

        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::AttachmentType::Color] = 1;
        expected_count[vk::Framebuffer::AttachmentType::DepthStencil] = 1;
        expected_count[vk::Framebuffer::AttachmentType::Resolve] = 1;
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
        vierkant::Device::create_info_t device_info = {};
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        // manually creating attachments
        // 1 color attachment (MSAA) | depth/stencil (MSAA) | resolve

        // color
        vk::Image::Format color_fmt;
        color_fmt.extent = fb_size;
        color_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        color_fmt.sample_count = device->max_usable_samples();
        auto color_img = vk::Image::create(device, color_fmt);

        // depth/stencil
        vk::Image::Format depth_stencil_fmt;
        depth_stencil_fmt.extent = fb_size;
        depth_stencil_fmt.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_stencil_fmt.sample_count = device->max_usable_samples();
        depth_stencil_fmt.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        depth_stencil_fmt.aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        auto depth_stencil_img = vk::Image::create(device, depth_stencil_fmt);

        vk::Image::Format resolve_fmt;
        resolve_fmt.extent = fb_size;
        resolve_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        resolve_fmt.sample_count = VK_SAMPLE_COUNT_1_BIT;
        auto resolve_img = vk::Image::create(device, resolve_fmt);

        vk::Framebuffer::AttachmentMap attachments = {
                {vk::Framebuffer::AttachmentType::Color, {color_img}},
                {vk::Framebuffer::AttachmentType::DepthStencil, {depth_stencil_img}},
                {vk::Framebuffer::AttachmentType::Resolve, {resolve_img}}
        };

        auto framebuffer = vk::Framebuffer(device, attachments);
        BOOST_CHECK(framebuffer);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::Color), 1);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::Resolve), 1);
        BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::Framebuffer::AttachmentType::DepthStencil), 1);
        attachment_count_t expected_count;
        expected_count[vk::Framebuffer::AttachmentType::Color] = 1;
        expected_count[vk::Framebuffer::AttachmentType::DepthStencil] = 1;
        expected_count[vk::Framebuffer::AttachmentType::Resolve] = 1;
        auto res = check_attachment_count(framebuffer.attachments(), expected_count);
        BOOST_CHECK(res);

    }
}