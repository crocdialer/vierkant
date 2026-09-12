#include "test_context.hpp"
#include <unordered_map>

#include "vierkant/PipelineCache.hpp"
#include "vierkant/shaders_slang.hpp"
#include "vierkant/vierkant.hpp"

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
    fmt.shader_stages = vierkant::create_shader_stages(vierkant::ShaderType::UNLIT_TEXTURE);

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
TEST(TestPipeline, RaytracingFormat)
{
    auto shader_module = vierkant::create_shader_module(vierkant::slang_shaders::slang::raypipeline_slang);
    ASSERT_TRUE(shader_module.create_info.codeSize);

    // the raypipeline provides more than one miss-shader
    ASSERT_TRUE(shader_module.entry_points.contains(VK_SHADER_STAGE_MISS_BIT_KHR));
    ASSERT_GT(shader_module.entry_points.at(VK_SHADER_STAGE_MISS_BIT_KHR).size(), 1);

    std::hash<vierkant::raytracing_pipeline_info_t> fmt_hash;

    vierkant::raytracing_pipeline_info_t foo = {}, bar = {};
    EXPECT_TRUE(foo == bar);
    EXPECT_TRUE(fmt_hash(foo) == fmt_hash(bar));

    foo.shader_stages = {{VK_SHADER_STAGE_RAYGEN_BIT_KHR, shader_module},
                         {VK_SHADER_STAGE_MISS_BIT_KHR, shader_module}};
    foo.hit_groups = {{.closest_hit = shader_module, .any_hit = shader_module}};
    bar = foo;
    EXPECT_TRUE(foo == bar);
    EXPECT_TRUE(fmt_hash(foo) == fmt_hash(bar));

    // same module, different entry-point -> a different pipeline
    auto miss_environment = shader_module;
    miss_environment.entry_point_name = "miss_environment";
    bar.shader_stages.find(VK_SHADER_STAGE_MISS_BIT_KHR)->second = miss_environment;
    EXPECT_TRUE(foo != bar);
    EXPECT_TRUE(fmt_hash(foo) != fmt_hash(bar));

    // an added intersection-shader turns a triangle hit-group into a procedural one
    bar = foo;
    bar.hit_groups.front().intersection = shader_module;
    EXPECT_TRUE(foo != bar);
    EXPECT_TRUE(fmt_hash(foo) != fmt_hash(bar));
}
