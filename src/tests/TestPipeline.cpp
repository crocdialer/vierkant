#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include <unordered_map>

#include "../vierkant.hpp"

BOOST_AUTO_TEST_CASE(TestPipeline_Format)
{
    vk::Pipeline::Format foo, bar;
    BOOST_CHECK(foo == bar);

    // hashing
    std::hash<vk::Pipeline::Format> fmt_hash;
    BOOST_CHECK(fmt_hash(foo) == fmt_hash(bar));

    bar.blending = true;
    BOOST_CHECK(foo != bar);
    BOOST_CHECK(fmt_hash(foo) != fmt_hash(bar));

    foo = bar;
    bar.primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    BOOST_CHECK(foo != bar);
    BOOST_CHECK(fmt_hash(foo) != fmt_hash(bar));

    std::unordered_map<vk::Pipeline::Format, int> pipeline_map;
    pipeline_map[foo] = 11;
    pipeline_map[bar] = 23;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestPipeline_Constructor)
{
    vk::Pipeline pipeline;
    BOOST_CHECK(!pipeline);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestPipeline_SingleColorDepth)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vk::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        auto device = vk::Device::create(physical_device,
                                         instance.use_validation_layers(),
                                         VK_NULL_HANDLE);

        auto framebuffer = vk::Framebuffer(device, fb_size);

        vk::Pipeline::Format fmt;
        fmt.viewport.width = framebuffer.extent().width;
        fmt.viewport.height = framebuffer.extent().height;
        fmt.renderpass = framebuffer.renderpass().get();

        fmt.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] = create_shader_module(device, vk::shaders::default_vert);
        fmt.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] = create_shader_module(device, vk::shaders::default_frag);
        auto pipeline = vk::Pipeline(device, fmt);
        BOOST_CHECK(pipeline);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
