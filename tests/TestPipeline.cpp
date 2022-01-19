#define BOOST_TEST_MAIN

#include "test_context.hpp"

#include <unordered_map>

#include "vierkant/vierkant.hpp"

BOOST_AUTO_TEST_CASE(TestPipeline_Format)
{
    vierkant::graphics_pipeline_info_t foo, bar;
    BOOST_CHECK(foo == bar);

    // hashing
    std::hash<vierkant::graphics_pipeline_info_t> fmt_hash;
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
    bar = {};
    foo = {};
    bar.scissor.extent.width = 200;
    bar.dynamic_states = {};
    BOOST_CHECK(foo != bar);

    // dynamic scissor
    foo.dynamic_states = bar.dynamic_states = {VK_DYNAMIC_STATE_SCISSOR};
    BOOST_CHECK(foo == bar);

    std::unordered_map<vierkant::graphics_pipeline_info_t, int> pipeline_map;
    pipeline_map[foo] = 11;
    pipeline_map[bar] = 23;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(TestPipeline_SingleColorDepth)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);

    vierkant::graphics_pipeline_info_t fmt;
    fmt.viewport.width = framebuffer.extent().width;
    fmt.viewport.height = framebuffer.extent().height;
    fmt.renderpass = framebuffer.renderpass().get();
    fmt.shader_stages = vierkant::create_shader_stages(test_context.device, vierkant::ShaderType::UNLIT_TEXTURE);
    auto pipeline = vierkant::Pipeline::create(test_context.device, fmt);
    BOOST_CHECK(pipeline);

    // expected error here
    BOOST_CHECK(test_context.validation_data.num_errors);
    test_context.validation_data.reset();


}

///////////////////////////////////////////////////////////////////////////////////////////////////
