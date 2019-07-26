#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include <unordered_map>

#include "vierkant/vierkant.hpp"

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

    // different viewport and not dynamic
    bar = foo;
    bar.viewport.x = 23;
    bar.dynamic_states = {};
    BOOST_CHECK(foo != bar);

    // dynamic viewport
    bar.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT};
    BOOST_CHECK(foo == bar);

    // different scissor and not dynamic
    bar = {}; foo = {};
    bar.scissor.extent.width = 200;
    bar.dynamic_states = {};
    BOOST_CHECK(foo != bar);

    // dynamic scissor
    foo.dynamic_states = bar.dynamic_states = {VK_DYNAMIC_STATE_SCISSOR};
    BOOST_CHECK(foo == bar);

    std::unordered_map<vk::Pipeline::Format, int> pipeline_map;
    pipeline_map[foo] = 11;
    pipeline_map[bar] = 23;
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
        fmt.shader_stages = vk::create_shader_stages(device, vk::ShaderType::UNLIT_TEXTURE);
        auto pipeline = vk::Pipeline::create(device, fmt);
        BOOST_CHECK(pipeline);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
