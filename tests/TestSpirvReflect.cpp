#include "test_context.hpp"
#include "vierkant/pipeline_formats.hpp"
#include "vierkant/shaders.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestSpirvReflect, GroupCount)
{
    vulkan_test_context_t test_context;

    // overlay-compute
    auto shader_stage = vierkant::create_shader_module(vierkant::shaders::renderer::object_overlay_comp);
    EXPECT_TRUE(shader_stage.create_info.codeSize);
    EXPECT_TRUE(shader_stage.entry_points.contains(VK_SHADER_STAGE_COMPUTE_BIT));
    EXPECT_FALSE(shader_stage.entry_points.at(VK_SHADER_STAGE_COMPUTE_BIT).empty());
    EXPECT_TRUE(shader_stage.entry_points.at(VK_SHADER_STAGE_COMPUTE_BIT).front().group_count);
    glm::uvec3 local_sizes = *shader_stage.entry_points.at(VK_SHADER_STAGE_COMPUTE_BIT).front().group_count;
    EXPECT_EQ(local_sizes, glm::uvec3(32, 32, 1));
}