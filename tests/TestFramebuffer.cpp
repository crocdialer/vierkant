#define BOOST_TEST_MAIN

#include "test_context.hpp"

#include "vierkant/vierkant.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

using attachment_count_t = std::map<vierkant::AttachmentType, uint32_t>;

bool check_attachment_count(const vierkant::attachment_map_t &attachments,
                            std::map<vierkant::AttachmentType, uint32_t> &expected_count)
{
    if(attachments.count(vierkant::AttachmentType::Color) !=
       expected_count.count(vierkant::AttachmentType::Color)){ return false; }
    if(attachments.count(vierkant::AttachmentType::Resolve) !=
       expected_count.count(vierkant::AttachmentType::Resolve)){ return false; }
    if(attachments.count(vierkant::AttachmentType::DepthStencil) !=
       expected_count.count(vierkant::AttachmentType::DepthStencil)){ return false; }

    for(auto &pair : attachments)
    {
        if(pair.second.size() != expected_count[pair.first]){ return false; }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestFramebuffer_Constructor)
{
    vierkant::Framebuffer framebuffer;
    BOOST_CHECK(!framebuffer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColor)
{
    vulkan_test_context_t test_context;

    VkExtent3D fb_size = {1920, 1080, 1};

    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;

    // only 1 color attachment, no depth/stencil
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);
    BOOST_CHECK(framebuffer);
    BOOST_CHECK_EQUAL(framebuffer.extent().width, fb_size.width);
    BOOST_CHECK_EQUAL(framebuffer.extent().height, fb_size.height);
    BOOST_CHECK_EQUAL(framebuffer.extent().depth, fb_size.depth);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(), 1);

    attachment_count_t expected_count;
    expected_count[vierkant::AttachmentType::Color] = 1;
    auto res = check_attachment_count(framebuffer.attachments(), expected_count);
    BOOST_CHECK(res);

    // record starting a renderpass

    // set clear values
    framebuffer.clear_color = {0.f, 0.f, 0.f, 1.f};
    framebuffer.clear_depth_stencil = {1.f, 0};

    // reset Framebuffer object
    framebuffer = vierkant::Framebuffer();
    BOOST_CHECK(!framebuffer);

}

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColorDepth)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    // 1 color attachment plus depth
    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;
    create_info.depth = true;
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);
    BOOST_CHECK(framebuffer);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::Color), 1);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::DepthStencil), 1);

    attachment_count_t expected_count;
    expected_count[vierkant::AttachmentType::Color] = 1;
    expected_count[vierkant::AttachmentType::DepthStencil] = 1;
    auto res = check_attachment_count(framebuffer.attachments(), expected_count);
    BOOST_CHECK(res);
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColorDepthStencil)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    // 1 color attachment plus depth/stencil
    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;
    create_info.depth = true;
    create_info.stencil = true;
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);
    BOOST_CHECK(framebuffer);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::Color), 1);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::DepthStencil), 1);

    attachment_count_t expected_count;
    expected_count[vierkant::AttachmentType::Color] = 1;
    expected_count[vierkant::AttachmentType::DepthStencil] = 1;
    auto res = check_attachment_count(framebuffer.attachments(), expected_count);
    BOOST_CHECK(res);
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_MultiColorDepthStencil)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    // 4 color attachments plus depth/stencil
    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;
    create_info.num_color_attachments = 4;
    create_info.depth = true;
    create_info.stencil = true;
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);
    BOOST_CHECK(framebuffer);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::Color), 4);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::DepthStencil), 1);

    attachment_count_t expected_count;
    expected_count[vierkant::AttachmentType::Color] = 4;
    expected_count[vierkant::AttachmentType::DepthStencil] = 1;
    auto res = check_attachment_count(framebuffer.attachments(), expected_count);
    BOOST_CHECK(res);
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_SingleColorDepthStencil_MSAA)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    // 1 color attachment (MSAA) | depth/stencil (MSAA) | resolve
    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;
    create_info.num_color_attachments = 1;
    create_info.depth = true;
    create_info.stencil = true;
    create_info.color_attachment_format.sample_count = test_context.device->max_usable_samples();
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);
    BOOST_CHECK(framebuffer);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::Color), 1);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::Resolve), 1);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::DepthStencil), 1);

    attachment_count_t expected_count;
    expected_count[vierkant::AttachmentType::Color] = 1;
    expected_count[vierkant::AttachmentType::DepthStencil] = 1;
    expected_count[vierkant::AttachmentType::Resolve] = 1;
    auto res = check_attachment_count(framebuffer.attachments(), expected_count);
    BOOST_CHECK(res);
}

BOOST_AUTO_TEST_CASE(TestFramebuffer_Manual_Attachments)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    // manually creating attachments
    // 1 color attachment (MSAA) | depth/stencil (MSAA) | resolve

    // color
    vierkant::Image::Format color_fmt;
    color_fmt.extent = fb_size;
    color_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    color_fmt.sample_count = test_context.device->max_usable_samples();
    auto color_img = vierkant::Image::create(test_context.device, color_fmt);

    // depth/stencil
    vierkant::Image::Format depth_stencil_fmt;
    depth_stencil_fmt.extent = fb_size;
    depth_stencil_fmt.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_stencil_fmt.sample_count = test_context.device->max_usable_samples();
    depth_stencil_fmt.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    depth_stencil_fmt.aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    auto depth_stencil_img = vierkant::Image::create(test_context.device, depth_stencil_fmt);

    vierkant::Image::Format resolve_fmt;
    resolve_fmt.extent = fb_size;
    resolve_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    resolve_fmt.sample_count = VK_SAMPLE_COUNT_1_BIT;
    auto resolve_img = vierkant::Image::create(test_context.device, resolve_fmt);

    vierkant::attachment_map_t attachments = {
            {vierkant::AttachmentType::Color,        {color_img}},
            {vierkant::AttachmentType::DepthStencil, {depth_stencil_img}},
            {vierkant::AttachmentType::Resolve,      {resolve_img}}
    };

    auto framebuffer = vierkant::Framebuffer(test_context.device, attachments);
    BOOST_CHECK(framebuffer);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::Color), 1);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::Resolve), 1);
    BOOST_CHECK_EQUAL(framebuffer.num_attachments(vierkant::AttachmentType::DepthStencil), 1);
    attachment_count_t expected_count;
    expected_count[vierkant::AttachmentType::Color] = 1;
    expected_count[vierkant::AttachmentType::DepthStencil] = 1;
    expected_count[vierkant::AttachmentType::Resolve] = 1;
    auto res = check_attachment_count(framebuffer.attachments(), expected_count);
    BOOST_CHECK(res);
}