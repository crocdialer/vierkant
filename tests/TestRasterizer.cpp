#include "test_context.hpp"
#include "vierkant/model/model_loading.hpp"
#include "vierkant/vierkant.hpp"

std::vector<vierkant::drawable_t> create_test_drawables(const vierkant::DevicePtr &device)
{

    // create some drawables for a template-shape
    vierkant::Mesh::entry_create_info_t entry_info = {};
    entry_info.geometry = vierkant::Geometry::Box();
    entry_info.geometry->normals.clear();
    entry_info.geometry->tangents.clear();
    entry_info.geometry->tex_coords.clear();

    // use sub-entry information to create a mesh (owns a combined + interleaved vertex-buffer)
    vierkant::Mesh::create_info_t mesh_create_info = {};
    mesh_create_info.mesh_buffer_params.pack_vertices = false;
    mesh_create_info.mesh_buffer_params.use_vertex_colors = true;
    auto mesh = vierkant::Mesh::create_with_entries(device, {entry_info}, mesh_create_info);

    EXPECT_EQ(1, mesh->entries.size());
    EXPECT_EQ(1, mesh->materials.size());

    vierkant::create_drawables_params_t drawable_params = {};
    auto drawables = vierkant::create_drawables({mesh}, drawable_params);

    // manually inject shader-stages which cannot be just guessed by above utility
    auto unlit_shader_stages = vierkant::create_shader_stages(device, vierkant::ShaderType::UNLIT_COLOR);
    for(auto &drawable: drawables){ drawable.pipeline_format.shader_stages = unlit_shader_stages; }
    return drawables;
}

TEST(Rasterizer, renderpass_API)
{
    vulkan_test_context_t test_context;

    const glm::vec2 res(1920, 1080);

    vierkant::Rasterizer::create_info_t create_info = {};
    create_info.num_frames_in_flight = 1;
    create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    create_info.viewport = {0.f, 0.f, res.x, res.y, 0.f, 1.f};
    auto rasterizer = vierkant::Rasterizer(test_context.device, create_info);
    auto drawables = create_test_drawables(test_context.device);

    // create a framebuffer to submit to
    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(res.x), static_cast<uint32_t>(res.y), 1};

    vierkant::Framebuffer framebuffer(test_context.device, framebuffer_info);

    // stage drawables and generate a (secondary) command-buffer
    rasterizer.stage_drawables(drawables);
    VkCommandBuffer secondaryCmdBuffer = rasterizer.render(framebuffer);

    EXPECT_TRUE(secondaryCmdBuffer);

    // now submit this command-buffer into a render-pass
    framebuffer.submit({secondaryCmdBuffer}, test_context.device->queue());

    // sync before exit, for good measure
    framebuffer.wait_fence();
}

TEST(Rasterizer, direct_API)
{
    vulkan_test_context_t test_context;

    const glm::vec2 res(1920, 1080);

    auto command_pool = vierkant::create_command_pool(test_context.device, vierkant::Device::Queue::GRAPHICS, 0);
    vierkant::Rasterizer::create_info_t create_info = {};
    create_info.num_frames_in_flight = 1;
    create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    create_info.viewport = {0.f, 0.f, res.x, res.y, 0.f, 1.f};
    create_info.command_pool = command_pool;

    auto rasterizer = vierkant::Rasterizer(test_context.device, create_info);
    auto drawables = create_test_drawables(test_context.device);

    // create a framebuffer to submit to
    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(res.x), static_cast<uint32_t>(res.y), 1};
    vierkant::Framebuffer framebuffer(test_context.device, framebuffer_info);

    // stage drawables
    rasterizer.stage_drawables(drawables);

    auto cmd_buffer = vierkant::CommandBuffer({test_context.device, command_pool.get()});
    cmd_buffer.begin();
    vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
    begin_rendering_info.commandbuffer = cmd_buffer.handle();
    framebuffer.begin_rendering(begin_rendering_info);

    vierkant::Rasterizer::rendering_info_t rendering_info = {};
    rendering_info.command_buffer = cmd_buffer.handle();
    rendering_info.color_attachment_formats = {framebuffer_info.color_attachment_format.format};

    // record drawing commands into an active command-buffer
    rasterizer.render(rendering_info);
    framebuffer.end_rendering();

    cmd_buffer.submit(test_context.device->queue(), true);
}