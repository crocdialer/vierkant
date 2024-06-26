#include "test_context.hpp"
#include "vierkant/PBRDeferred.hpp"
#include "vierkant/model/model_loading.hpp"
#include "vierkant/vierkant.hpp"

TEST(TestPBRDeferred, basic)
{
    // NOTE: all of these are in fact optional, but validation would complain otherwise
    std::vector<const char *> extensions = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                            VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME};
    vulkan_test_context_t test_context(extensions);
    const glm::vec2 res(1920, 1080);

    vierkant::Rasterizer::create_info_t create_info = {};
    create_info.num_frames_in_flight = 1;
    create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    create_info.viewport = {0.f, 0.f, res.x, res.y, 0.f, 1.f};

    auto renderer = vierkant::Rasterizer(test_context.device, create_info);

    // create some drawables for a template-shape
    vierkant::Mesh::entry_create_info_t entry_info = {};
    entry_info.geometry = vierkant::Geometry::Box();

    // use sub-entry information to create a mesh (owns a combined + interleaved vertex-buffer)
    vierkant::Mesh::create_info_t mesh_create_info = {};
    auto mesh = vierkant::Mesh::create_with_entries(test_context.device, {entry_info}, mesh_create_info);

    EXPECT_EQ(1, mesh->entries.size());
    EXPECT_EQ(1, mesh->materials.size());

    // create camera / mesh-node/ scene
    auto registry = std::make_shared<entt::registry>();
    auto cam = vierkant::PerspectiveCamera::create(registry);
    EXPECT_TRUE(cam);

    auto scene = vierkant::Scene::create();
    auto mesh_node = scene->create_mesh_object({mesh});
    EXPECT_TRUE(mesh_node);

    scene->add_object(mesh_node);
    EXPECT_TRUE(scene);

    // create PBR scene-renderer
    vierkant::PBRDeferred::create_info_t pbr_render_info = {};

    // this must be >= 2 because history-buffers are used
    pbr_render_info.num_frames_in_flight = 2;
    pbr_render_info.pipeline_cache = nullptr;
    pbr_render_info.settings.resolution = res;
    pbr_render_info.settings.indirect_draw = false;
    auto pbr_renderer = vierkant::PBRDeferred::create(test_context.device, pbr_render_info);
    EXPECT_TRUE(pbr_renderer);

    // create a framebuffer to submit to
    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(res.x), static_cast<uint32_t>(res.y), 1};
    vierkant::Framebuffer framebuffer(test_context.device, framebuffer_info);

    // stage drawables and generate a (secondary) command-buffer
    auto render_result = pbr_renderer->render_scene(renderer, scene, cam, {});
    VkCommandBuffer secondaryCmdBuffer = renderer.render(framebuffer);
    EXPECT_EQ(render_result.num_draws, 1);
    EXPECT_TRUE(secondaryCmdBuffer);

    // now submit this command-buffer into a render-pass
    framebuffer.submit({secondaryCmdBuffer}, test_context.device->queue(), render_result.semaphore_infos);

    // sync before exit, for good measure
    framebuffer.wait_fence();
}