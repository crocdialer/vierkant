#include "test_context.hpp"
#include "vierkant/PBRDeferred.hpp"
#include "vierkant/cubemap_utils.hpp"
#include "vierkant/model/model_loading.hpp"
#include "vierkant/vierkant.hpp"

TEST(TestPBRDeferred, basic)
{
    // NOTE: all of these are in fact optional, but validation would complain otherwise
    std::vector<const char *> extensions = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                            VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME};
    vulkan_test_context_t test_context(extensions);

    // g-buffer uses fragment-shader barycentric intrinsics; drivers lacking it (e.g. lavapipe) coredump
    if(!vierkant::check_device_extension_support(test_context.instance.physical_devices()[0],
                                                 {VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME}))
    {
        GTEST_SKIP() << "device lacks VK_KHR_fragment_shader_barycentric; skipping";
    }
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
    EXPECT_EQ(1, mesh->material_ids.size());

    // create camera / mesh-node/ scene
    auto scene = vierkant::Scene::create();

    auto cam = scene->create_object();
    cam->add_component<vierkant::camera_component_t>();
    EXPECT_TRUE(cam);

    auto mesh_node = scene->create_mesh_object({mesh});
    EXPECT_TRUE(mesh_node);

    scene->add_object(mesh_node);
    EXPECT_TRUE(scene);

    // an environment plus both convolutions enable the lighting-pass and with it the skybox-pass,
    // which binds a SamplerCube in set 0
    constexpr auto hdr_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    auto env_img = vierkant::cubemap_neutral_environment(test_context.device, 256, test_context.device->queue(), true,
                                                         hdr_format);
    scene->set_environment(env_img);
    EXPECT_TRUE(scene->environment());

    // create PBR scene-renderer
    vierkant::PBRDeferred::create_info_t pbr_render_info = {};

    // this must be >= 2 because history-buffers are used
    pbr_render_info.num_frames_in_flight = 2;
    pbr_render_info.pipeline_cache = nullptr;
    pbr_render_info.settings.resolution = res;
    pbr_render_info.settings.indirect_draw = false;
    pbr_render_info.conv_lambert =
            vierkant::create_convolution_lambert(test_context.device, env_img, 64, hdr_format,
                                                 test_context.device->queue());
    pbr_render_info.conv_ggx = vierkant::create_convolution_ggx(test_context.device, env_img, env_img->width(), hdr_format,
                                                                test_context.device->queue());
    // NOTE: both convolutions come back in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, sampling them needs this
    pbr_render_info.conv_lambert->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    pbr_render_info.conv_ggx->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    auto pbr_renderer = vierkant::PBRDeferred::create(test_context.device, pbr_render_info);
    EXPECT_TRUE(pbr_renderer);

    // create a framebuffer to submit to
    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(res.x), static_cast<uint32_t>(res.y), 1};
    vierkant::Framebuffer framebuffer(test_context.device, framebuffer_info);

    // stage drawables and generate a (secondary) command-buffer
    auto render_result = pbr_renderer->render_scene(renderer, scene, cam, vierkant::LAYER_ALL);
    VkCommandBuffer secondaryCmdBuffer = renderer.render(framebuffer);
    EXPECT_EQ(render_result.num_draws, 1);
    EXPECT_TRUE(secondaryCmdBuffer);

    // now submit this command-buffer into a render-pass
    framebuffer.submit({secondaryCmdBuffer}, test_context.device->queue(), render_result.semaphore_infos);

    // sync before exit, for good measure
    framebuffer.wait_fence();
}