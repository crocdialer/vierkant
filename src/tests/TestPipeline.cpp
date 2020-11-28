#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#include <unordered_map>

#include "vierkant/vierkant.hpp"

BOOST_AUTO_TEST_CASE(TestPipeline_Format)
{
    vierkant::Pipeline::Format foo, bar;
    BOOST_CHECK(foo == bar);

    // hashing
    std::hash<vierkant::Pipeline::Format> fmt_hash;
    BOOST_CHECK(fmt_hash(foo) == fmt_hash(bar));

    bar.blend_state.blendEnable = true;
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

    std::unordered_map<vierkant::Pipeline::Format, int> pipeline_map;
    pipeline_map[foo] = 11;
    pipeline_map[bar] = 23;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestPipeline_SingleColorDepth)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    bool use_validation = true;
    vierkant::Instance instance(use_validation, {});

    for(auto physical_device : instance.physical_devices())
    {
        vierkant::Device::create_info_t device_info = {};
        device_info.instance = instance.handle();
        device_info.physical_device = physical_device;
        device_info.use_validation = instance.use_validation_layers();
        auto device = vk::Device::create(device_info);

        vierkant::Framebuffer::create_info_t create_info = {};
        create_info.size = fb_size;
        auto framebuffer = vierkant::Framebuffer(device, create_info);

        vierkant::Pipeline::Format fmt;
        fmt.viewport.width = framebuffer.extent().width;
        fmt.viewport.height = framebuffer.extent().height;
        fmt.renderpass = framebuffer.renderpass().get();
        fmt.shader_stages = vierkant::create_shader_stages(device, vierkant::ShaderType::UNLIT_TEXTURE);
        auto pipeline = vierkant::Pipeline::create(device, fmt);
        BOOST_CHECK(pipeline);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
