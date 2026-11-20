// scene-renderers through descriptor-sets and through a descriptor-heap must produce identical frames.

#include "heap_test_utils.hpp"
#include <vierkant/PBRDeferred.hpp>
#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/cubemap_utils.hpp>
#include <vierkant/vierkant.hpp>

using namespace heap_test;

namespace
{

constexpr uint32_t width = 160, height = 90;

// optional, but validation complains without them (same as TestPBRDeferred)
const std::vector<const char *> scene_extensions = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                                    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                                    VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME};

struct frame_t
{
    std::vector<uint8_t> color;
    uint32_t num_draws = 0;
};

//! a box in front of a camera, with an environment
vierkant::ScenePtr create_scene(const vierkant::DevicePtr &device, vierkant::Object3DPtr &cam)
{
    vierkant::Mesh::entry_create_info_t entry_info = {};
    entry_info.geometry = vierkant::Geometry::Box();

    // the path-tracer builds acceleration-structures from the mesh-buffers
    vierkant::Mesh::create_info_t mesh_info = {};
    mesh_info.buffer_usage_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    auto mesh = vierkant::Mesh::create_with_entries(device, {entry_info}, mesh_info);

    auto scene = vierkant::Scene::create();
    cam = scene->create_object();
    cam->add_component<vierkant::camera_component_t>();
    vierkant::transform_t cam_transform = {};
    cam_transform.translation = {0.5f, 0.75f, 3.f};
    cam->set_transform(cam_transform);

    auto mesh_node = scene->create_mesh_object({mesh});
    vierkant::transform_t mesh_transform = {};
    mesh_transform.rotation = glm::quat(glm::vec3(0.3f, 0.6f, 0.f));
    mesh_node->set_transform(mesh_transform);
    scene->add_object(mesh_node);

    scene->set_environment(
            vierkant::cubemap_neutral_environment(device, 64, device->queue(), true, VK_FORMAT_R16G16B16A16_SFLOAT));
    return scene;
}

//! renders num_frames frames and reads back the last one
frame_t render(const vierkant::DevicePtr &device, const vierkant::PipelineCachePtr &cache,
               const vierkant::ScenePtr &scene, const vierkant::Object3DPtr &cam,
               vierkant::SceneRenderer &scene_renderer, uint32_t num_frames)
{
    vierkant::Rasterizer::create_info_t create_info = {};
    create_info.num_frames_in_flight = 1;
    create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    create_info.viewport = {0.f, 0.f, float(width), float(height), 0.f, 1.f};
    create_info.pipeline_cache = cache;
    vierkant::Rasterizer renderer(device, create_info);

    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {width, height, 1};
    framebuffer_info.color_attachment_format.format = VK_FORMAT_R8G8B8A8_UNORM;
    framebuffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    vierkant::Framebuffer framebuffer(device, framebuffer_info);

    frame_t ret;
    for(uint32_t i = 0; i < num_frames; ++i)
    {
        auto render_result = scene_renderer.render_scene(renderer, scene, cam, vierkant::LAYER_ALL);
        VkCommandBuffer cmd = renderer.render(framebuffer);
        framebuffer.submit({cmd}, device->queue(), render_result.semaphore_infos);
        framebuffer.wait_fence();
        ret.num_draws = render_result.num_draws;
    }
    vkDeviceWaitIdle(device->handle());
    ret.color = read_image(device, framebuffer.color_attachment(0), 0);
    return ret;
}

}// namespace

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(SceneHeap, pbr_deferred)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    auto results = per_path(
            [&](const vierkant::DevicePtr &device, const vierkant::PipelineCachePtr &cache) {
                // the g-buffer needs fragment-shader barycentrics
                if(!vierkant::check_device_extension_support(device->physical_device(),
                                                             {VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME}))
                {
                    return frame_t{};
                }
                // the scene owns the object-pool, it has to outlive the camera
                vierkant::ScenePtr scene;
                vierkant::Object3DPtr cam;
                scene = create_scene(device, cam);

                constexpr auto hdr_format = VK_FORMAT_R16G16B16A16_SFLOAT;
                auto env = scene->environment();
                vierkant::PBRDeferred::create_info_t pbr_info = {};
                pbr_info.num_frames_in_flight = 2;
                pbr_info.pipeline_cache = cache;
                pbr_info.settings.resolution = {width, height};

                // indirect drawing copies into a buffer without TRANSFER_DST, on both paths
                pbr_info.settings.indirect_draw = false;
                pbr_info.conv_lambert =
                        vierkant::create_convolution_lambert(device, env, 32, hdr_format, device->queue());
                pbr_info.conv_ggx =
                        vierkant::create_convolution_ggx(device, env, env->width(), hdr_format, device->queue());
                pbr_info.conv_lambert->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                pbr_info.conv_ggx->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                auto pbr_renderer = vierkant::PBRDeferred::create(device, pbr_info);
                return render(device, cache, scene, cam, *pbr_renderer, 3);
            },
            scene_extensions);

    if(results[0].color.empty()) { GTEST_SKIP() << "device lacks VK_KHR_fragment_shader_barycentric"; }
    EXPECT_EQ(results[0].num_draws, 1);
    EXPECT_EQ(results[1].num_draws, 1);
    EXPECT_FALSE(all_zero(results[1].color));
    EXPECT_TRUE(results[0].color == results[1].color) << diff_stats(results[0].color, results[1].color);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(SceneHeap, pbr_path_tracer)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    auto results = per_path(
            [&](const vierkant::DevicePtr &device, const vierkant::PipelineCachePtr &cache) {
                if(!vierkant::check_device_extension_support(device->physical_device(),
                                                             vierkant::RayTracer::required_extensions()))
                {
                    return frame_t{};
                }
                // the scene owns the object-pool, it has to outlive the camera
                vierkant::ScenePtr scene;
                vierkant::Object3DPtr cam;
                scene = create_scene(device, cam);

                // denoising off: its output varies between runs on the descriptor-set path alone
                vierkant::PBRPathTracer::create_info_t path_tracer_info = {};
                path_tracer_info.num_frames_in_flight = 1;
                path_tracer_info.pipeline_cache = cache;
                path_tracer_info.seed = 1234;
                path_tracer_info.settings.resolution = {width, height};
                path_tracer_info.settings.denoising = false;

                // stop after a fixed number of batches. resets (e.g. acceleration-structure builds) land in
                // different frames per run, so the frame-count alone does not fix the accumulated state.
                path_tracer_info.settings.max_num_batches = 4;
                path_tracer_info.settings.suspend_trace_when_done = true;
                auto path_tracer = vierkant::PBRPathTracer::create(device, path_tracer_info);
                auto frame = render(device, cache, scene, cam, *path_tracer, 16);
                EXPECT_EQ(path_tracer->current_batch(), 4);
                return frame;
            },
            scene_extensions);

    if(results[0].color.empty()) { GTEST_SKIP() << "device lacks raytracing-pipelines"; }
    EXPECT_FALSE(all_zero(results[1].color));
    EXPECT_TRUE(results[0].color == results[1].color) << diff_stats(results[0].color, results[1].color);
}
