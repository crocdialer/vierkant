#include "test_context.hpp"
#include "vierkant/shaders.hpp"
#include "vierkant/pipeline_formats.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(TestSpirvReflect, GroupCount)
{
    vulkan_test_context_t test_context;

    // overlay-compute
    glm::uvec3 local_sizes;
    auto shader_stage = vierkant::create_shader_module(test_context.device, vierkant::shaders::pbr::object_overlay_comp,
                                                       &local_sizes);
    EXPECT_EQ(local_sizes, glm::uvec3(32, 32, 1));
}