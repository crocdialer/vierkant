#include "test_context.hpp"
#include "vierkant/AssetProvider.hpp"
#include "vierkant/PBRDeferred.hpp"
#include "vierkant/model/model_loading.hpp"
#include "vierkant/vierkant.hpp"

//! build a minimal model_assets_t: a box + one generated texture + a material referencing it
//! with a non-default sampler-override.
static vierkant::model::model_assets_t create_test_assets(vierkant::TextureId tex_id, vierkant::SamplerId sampler_id,
                                                          vierkant::MaterialId mat_id)
{
    vierkant::model::model_assets_t assets = {};

    vierkant::Mesh::entry_create_info_t entry_info = {};
    entry_info.geometry = vierkant::Geometry::Box();
    entry_info.material_index = 0;
    assets.geometry_data = std::vector{entry_info};

    // one generated texture
    assets.textures[tex_id] = crocore::Image_<uint8_t>::create(4, 4, 4);

    // a non-default sampler (default address-mode is REPEAT)
    vierkant::texture_sampler_t sampler = {};
    sampler.address_mode_u = vierkant::texture_sampler_t::AddressMode::CLAMP_TO_EDGE;
    sampler.address_mode_v = vierkant::texture_sampler_t::AddressMode::CLAMP_TO_EDGE;
    assets.texture_samplers[sampler_id] = sampler;

    // a material referencing the texture with the sampler-override
    vierkant::material_t material = {};
    material.id = mat_id;
    material.texture_data[vierkant::TextureType::Color] = {.texture_id = tex_id, .sampler_id = sampler_id};
    assets.materials = {material};

    return assets;
}

// exercises the load -> populate seam in-repo and regression-guards the {texture_id, sampler_id} store
TEST(TestAssetProvider, populate_sampler_override)
{
    vulkan_test_context_t test_context;

    const auto tex_id = vierkant::TextureId::random();
    const auto sampler_id = vierkant::SamplerId::random();
    const auto mat_id = vierkant::MaterialId::random();
    auto assets = create_test_assets(tex_id, sampler_id, mat_id);

    vierkant::model::load_mesh_params_t load_params = {};
    load_params.device = test_context.device;
    load_params.load_queue = test_context.device->queue();
    auto result = vierkant::model::load_mesh(load_params, assets);
    ASSERT_TRUE(result.mesh);

    // load_mesh should realize both the base {tex, nil} and the sampled permutation {tex, sampler}
    const vierkant::texture_key_t base_key = {tex_id, vierkant::SamplerId::nil()};
    const vierkant::texture_key_t sampled_key = {tex_id, sampler_id};
    ASSERT_TRUE(result.textures.contains(base_key));
    ASSERT_TRUE(result.textures.contains(sampled_key));
    ASSERT_TRUE(result.samplers.contains(sampler_id));

    auto provider = vierkant::AssetProvider::create(test_context.device);
    provider->populate(result);

    // material survived the merge unmodified
    const auto *material = provider->material(mat_id);
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(material->texture_data.at(vierkant::TextureType::Color).texture_id, tex_id);
    EXPECT_EQ(material->texture_data.at(vierkant::TextureType::Color).sampler_id, sampler_id);

    // both texture variants live in the provider, and the override is a distinct image/sampler
    auto base_img = provider->texture(base_key);
    auto sampled_img = provider->texture(sampled_key);
    ASSERT_TRUE(base_img);
    ASSERT_TRUE(sampled_img);
    EXPECT_NE(base_img, sampled_img);
    EXPECT_TRUE(sampled_img->sampler());
    EXPECT_NE(base_img->sampler(), sampled_img->sampler());

    // the provider owns the get-or-create'd sampler, baked onto the sampled image
    EXPECT_EQ(provider->sampler(sampler_id), sampled_img->sampler());

    // g-buffer uses fragment-shader barycentric intrinsics; drivers lacking it (e.g. lavapipe) coredump
    if(!vierkant::check_device_extension_support(test_context.instance.physical_devices()[0],
                                                 {VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME}))
    {
        GTEST_SKIP() << "device lacks VK_KHR_fragment_shader_barycentric; skipping render";
    }

    // drive the loaded assets through one PBRDeferred::render_scene
    const glm::vec2 res(512, 512);
    vierkant::Rasterizer::create_info_t rasterizer_info = {};
    rasterizer_info.num_frames_in_flight = 1;
    rasterizer_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    rasterizer_info.viewport = {0.f, 0.f, res.x, res.y, 0.f, 1.f};
    auto renderer = vierkant::Rasterizer(test_context.device, rasterizer_info);

    auto scene = vierkant::Scene::create({}, provider);
    auto cam = scene->create_object();
    cam->add_component<vierkant::camera_component_t>({vierkant::physical_camera_params_t{}});
    auto mesh_node = scene->create_mesh_object({result.mesh});
    scene->add_object(mesh_node);

    vierkant::PBRDeferred::create_info_t pbr_render_info = {};
    pbr_render_info.num_frames_in_flight = 2;
    pbr_render_info.settings.resolution = res;
    pbr_render_info.settings.indirect_draw = false;
    auto pbr_renderer = vierkant::PBRDeferred::create(test_context.device, pbr_render_info);
    ASSERT_TRUE(pbr_renderer);

    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {static_cast<uint32_t>(res.x), static_cast<uint32_t>(res.y), 1};
    vierkant::Framebuffer framebuffer(test_context.device, framebuffer_info);

    auto render_result = pbr_renderer->render_scene(renderer, scene, cam, {});
    VkCommandBuffer cmd_buffer = renderer.render(framebuffer);
    EXPECT_EQ(render_result.num_draws, 1);
    ASSERT_TRUE(cmd_buffer);

    framebuffer.submit({cmd_buffer}, test_context.device->queue(), render_result.semaphore_infos);
    framebuffer.wait_fence();
}
