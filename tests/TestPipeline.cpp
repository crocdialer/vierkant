#include "test_context.hpp"
#include <unordered_map>

#include "vierkant/vierkant.hpp"
#include "vierkant/PipelineCache.hpp"

TEST(TestPipeline, Format)
{
    vierkant::graphics_pipeline_info_t foo, bar;
    EXPECT_TRUE(foo == bar);

    // hashing
    std::hash<vierkant::graphics_pipeline_info_t> fmt_hash;
    EXPECT_TRUE(fmt_hash(foo) == fmt_hash(bar));

    bar.blend_state.blendEnable = true;
    EXPECT_TRUE(foo != bar);
    EXPECT_TRUE(fmt_hash(foo) != fmt_hash(bar));

    foo = bar;
    bar.primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    EXPECT_TRUE(foo != bar);
    EXPECT_TRUE(fmt_hash(foo) != fmt_hash(bar));

    // different viewport and not dynamic
    bar = foo;
    bar.viewport.x = 23;
    bar.dynamic_states = {};
    EXPECT_TRUE(foo != bar);

    // dynamic viewport
    bar.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT};
    EXPECT_TRUE(foo == bar);

    // different scissor and not dynamic
    bar = {};
    foo = {};
    bar.scissor.extent.width = 200;
    bar.dynamic_states = {};
    EXPECT_TRUE(foo != bar);

    // dynamic scissor
    foo.dynamic_states = bar.dynamic_states = {VK_DYNAMIC_STATE_SCISSOR};
    EXPECT_TRUE(foo == bar);

    std::unordered_map<vierkant::graphics_pipeline_info_t, int> pipeline_map;
    pipeline_map[foo] = 11;
    pipeline_map[bar] = 23;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestPipeline, SingleColorDepth)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);

    // TODO: we expected errors here already, but Mesa/radv 25 just crashes instead of returning an error :(

    // vierkant::graphics_pipeline_info_t fmt;
    // fmt.viewport.width = static_cast<float>(framebuffer.extent().width);
    // fmt.viewport.height = static_cast<float>(framebuffer.extent().height);
    // fmt.renderpass = framebuffer.renderpass().get();
    // fmt.shader_stages = vierkant::create_shader_stages(test_context.device, vierkant::ShaderType::UNLIT_TEXTURE);
    // auto pipeline = vierkant::Pipeline::create(test_context.device, fmt);
    // EXPECT_TRUE(pipeline);

    // TODO: expected error here, make this obsolete
    // EXPECT_TRUE(test_context.validation_data.num_errors);
    // test_context.validation_data = {};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestPipeline, PipelineCache)
{
    VkExtent3D fb_size = {1920, 1080, 1};

    vulkan_test_context_t test_context;

    vierkant::Framebuffer::create_info_t create_info = {};
    create_info.size = fb_size;
    auto framebuffer = vierkant::Framebuffer(test_context.device, create_info);

    vierkant::graphics_pipeline_info_t fmt;
    fmt.viewport.width = static_cast<float>(framebuffer.extent().width);
    fmt.viewport.height = static_cast<float>(framebuffer.extent().height);
    fmt.renderpass = framebuffer.renderpass().get();
    fmt.shader_stages = vierkant::create_shader_stages(test_context.device, vierkant::ShaderType::UNLIT_TEXTURE);

    // TODO: we expected errors here already, but Mesa/radv 25 just crashes instead of returning an error :(
    
    // auto cache = vierkant::PipelineCache::create(test_context.device);
    // auto pipeline = cache->pipeline(fmt);
    // EXPECT_TRUE(pipeline);
    // EXPECT_TRUE(cache->has(fmt));
    // EXPECT_TRUE(pipeline == cache->pipeline(fmt));

    // TODO: expected error here, make this obsolete
    // EXPECT_TRUE(test_context.validation_data.num_errors);
    // test_context.validation_data = {};
}