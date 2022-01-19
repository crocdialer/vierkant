#define BOOST_TEST_MAIN

#include "test_context.hpp"

#include "vierkant/vierkant.hpp"
#include "vierkant/model_loading.hpp"

BOOST_AUTO_TEST_CASE(TestRenderer)
{
    vulkan_test_context_t test_context;

    const glm::vec2 res(1920, 1080);

    vierkant::Renderer::create_info_t create_info = {};
    create_info.num_frames_in_flight = 1;
    create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    create_info.viewport = {0.f, 0.f, res.x,
                            res.y, 0.f, 1.f};

    auto renderer = vierkant::Renderer(test_context.device, create_info);

    // create some drawables for a template-shape
    vierkant::Mesh::entry_create_info_t entry_info = {};
    entry_info.geometry = vierkant::Geometry::Box();
    entry_info.geometry->normals.clear();
    entry_info.geometry->tangents.clear();
    entry_info.geometry->tex_coords.clear();

    vierkant::model::mesh_assets_t mesh_assets = {};
    mesh_assets.entry_create_infos = {entry_info};
    mesh_assets.materials.resize(1);

    // use sub-entry information to create a mesh (owns a combined + interleaved vertex-buffer)
    vierkant::Mesh::create_info_t mesh_create_info = {};
    auto mesh = vierkant::Mesh::create_with_entries(test_context.device, mesh_assets.entry_create_infos,
                                                    mesh_create_info);

    BOOST_CHECK_EQUAL(mesh_assets.entry_create_infos.size(), mesh->entries.size());
    BOOST_CHECK_EQUAL(mesh_assets.materials.size(), mesh->materials.size());

    auto drawables = vierkant::Renderer::create_drawables(mesh);
    BOOST_CHECK_EQUAL(mesh_assets.entry_create_infos.size(), drawables.size());

    // manually inject shader-stages which cannot be just guessed by above utility
    auto unlit_shader_stages = vierkant::create_shader_stages(test_context.device,
                                                              vierkant::ShaderType::UNLIT_COLOR);
    for(auto &drawable : drawables)
    {
        drawable.pipeline_format.shader_stages = unlit_shader_stages;
    }

    // create a framebuffer to submit to
    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(res.x), static_cast<uint32_t>(res.y), 1};

    vierkant::Framebuffer framebuffer(test_context.device, framebuffer_info);

    // stage drawables and generate a (secondary) command-buffer
    renderer.stage_drawables(drawables);
    VkCommandBuffer secondaryCmdBuffer = renderer.render(framebuffer);

    BOOST_CHECK(secondaryCmdBuffer);

    // now submit this command-buffer into a render-pass
    framebuffer.submit({secondaryCmdBuffer}, test_context.device->queue());

    // sync before exit, for good measure
    framebuffer.wait_fence();
}