#include <gtest/gtest.h>

#include <random>

#include <crocore/Image.hpp>
#include <vierkant/bundle.hpp>
#include <vierkant/vertex_splicer.hpp>
#include <vierkant/ziparchive.h>

namespace
{

vierkant::model::model_assets_t dummy_assets()
{
    vierkant::model::model_assets_t assets = {};

    vierkant::mesh_buffer_bundle_t bundle = {};
    bundle.vertex_stride = sizeof(vierkant::packed_vertex_t);
    bundle.num_materials = 1;
    bundle.vertex_buffer = {1, 2, 3, 4, 5, 6, 7, 8};
    bundle.index_buffer = {0, 1, 2};
    assets.geometry_data = std::move(bundle);

    vierkant::material_t material = {};
    material.name = "test-material";
    material.roughness = 0.25f;
    assets.materials = {material};

    // a single bone, covering nodes::node_t serialization
    auto root_bone = std::make_shared<vierkant::nodes::node_t>();
    root_bone->name = "root_bone";
    root_bone->id = vierkant::nodes::NodeId::from_name(root_bone->name);
    assets.root_bone = root_bone;
    return assets;
}

//! a scoped temp-directory, removed on destruction.
struct temp_dir_t
{
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("vierkant_bundle_test_" + std::to_string(std::random_device()()));
    temp_dir_t() { std::filesystem::create_directories(path); }
    ~temp_dir_t() { std::filesystem::remove_all(path); }
};

}// namespace

//____________________________________________________________________________//

TEST(Bundle, file_roundtrip)
{
    temp_dir_t dir;
    const auto path = dir.path / "assets.4km";
    const auto assets = dummy_assets();

    vierkant::save_bundle_file(assets, path);
    ASSERT_TRUE(std::filesystem::exists(path));

    auto loaded = vierkant::load_model_bundle_file(path);
    ASSERT_TRUE(loaded);
    ASSERT_EQ(loaded->materials.size(), 1);
    EXPECT_EQ(loaded->materials[0].name, "test-material");
    EXPECT_EQ(loaded->materials[0].roughness, 0.25f);

    ASSERT_TRUE(loaded->root_bone);
    EXPECT_EQ(loaded->root_bone->name, "root_bone");
    EXPECT_EQ(loaded->root_bone->id, vierkant::nodes::NodeId::from_name("root_bone"));

    const auto &bundle = std::get<vierkant::mesh_buffer_bundle_t>(loaded->geometry_data);
    EXPECT_EQ(bundle.vertex_stride, sizeof(vierkant::packed_vertex_t));
    EXPECT_EQ(bundle.vertex_buffer, std::vector<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}));
    EXPECT_EQ(bundle.index_buffer, std::vector<vierkant::index_t>({0, 1, 2}));
}

TEST(Bundle, zip_roundtrip)
{
    temp_dir_t dir;
    const auto path = dir.path / "assets.4km";
    const auto zip_path = dir.path / "assets.zip";

    vierkant::save_bundle_file(dummy_assets(), path, zip_path);

    // the plain file is removed once stored in the archive
    EXPECT_FALSE(std::filesystem::exists(path));
    ASSERT_TRUE(std::filesystem::exists(zip_path));
    EXPECT_TRUE(vierkant::ziparchive(zip_path).has_file("assets.4km"));

    // loading falls back to the archive when the plain file is absent
    auto loaded = vierkant::load_model_bundle_file(path, zip_path);
    ASSERT_TRUE(loaded);
    ASSERT_EQ(loaded->materials.size(), 1);
    EXPECT_EQ(loaded->materials[0].name, "test-material");
}

TEST(Bundle, filename_covers_layout_and_params)
{
    const std::filesystem::path model = "foo.glb";
    vierkant::mesh_buffer_params_t params = {};

    const auto base = vierkant::model_bundle_filename(model, params, false);
    EXPECT_TRUE(base.starts_with("foo.glb_"));
    EXPECT_TRUE(base.ends_with(".4km"));

    // deterministic
    EXPECT_EQ(base, vierkant::model_bundle_filename(model, params, false));

    // bake-parameters and texture-compression both participate
    params.generate_lods = !params.generate_lods;
    EXPECT_NE(base, vierkant::model_bundle_filename(model, params, false));
    EXPECT_NE(base, vierkant::model_bundle_filename(model, {}, true));
}

//____________________________________________________________________________//

namespace
{

vierkant::bcn::compress_result_t dummy_compressed()
{
    vierkant::bcn::compress_result_t result = {};
    result.mode = vierkant::bcn::BC7;
    result.base_width = 8;
    result.base_height = 8;
    result.levels = {{{{1, 2}}, {{3, 4}}}, {{{5, 6}}}};
    return result;
}

vierkant::cubemap_data_t dummy_cubemap()
{
    vierkant::cubemap_data_t data = {};
    data.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    data.size = 2;
    data.levels = {std::vector<uint8_t>(6 * 2 * 2 * 8, 0x42), std::vector<uint8_t>(6 * 1 * 1 * 8, 0x23)};
    return data;
}

}// namespace

TEST(Bundle, texture_file_roundtrip)
{
    temp_dir_t dir;
    const auto path = dir.path / "texture.4kt";

    vierkant::save_bundle_file(vierkant::texture_variant_t(dummy_compressed()), path);
    ASSERT_TRUE(std::filesystem::exists(path));

    auto loaded = vierkant::load_texture_bundle_file(path);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(std::holds_alternative<vierkant::bcn::compress_result_t>(*loaded));

    const auto &result = std::get<vierkant::bcn::compress_result_t>(*loaded);
    EXPECT_EQ(result.mode, vierkant::bcn::BC7);
    EXPECT_EQ(result.base_width, 8);
    EXPECT_EQ(result.levels.size(), 2);
    EXPECT_EQ(result.levels[0].size(), 2);
    EXPECT_EQ(result.levels[0][1].value[0], 3);
    EXPECT_EQ(result.levels[1].size(), 1);
}

TEST(Bundle, texture_uncompressed_roundtrip)
{
    temp_dir_t dir;
    const auto path = dir.path / "texture.4kt";

    // an uncompressed image is stored png-encoded, so the round-trip must preserve dimensions + texels
    auto img = crocore::Image_<uint8_t>::create(4, 4, 4);
    auto *texels = static_cast<uint8_t *>(const_cast<void *>(img->data()));
    std::fill(texels, texels + img->num_bytes(), 0x7f);

    vierkant::save_bundle_file(vierkant::texture_variant_t(img), path);

    auto loaded = vierkant::load_texture_bundle_file(path);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(std::holds_alternative<crocore::ImagePtr>(*loaded));

    const auto &loaded_img = std::get<crocore::ImagePtr>(*loaded);
    ASSERT_TRUE(loaded_img);
    EXPECT_EQ(loaded_img->width(), 4);
    EXPECT_EQ(loaded_img->height(), 4);
    EXPECT_EQ(loaded_img->num_components(), 4);
}

TEST(Bundle, environment_file_roundtrip)
{
    temp_dir_t dir;
    const auto path = dir.path / "environment.4ke";

    vierkant::environment_assets_t assets = {};
    assets.skybox = dummy_cubemap();
    assets.conv_lambert = dummy_cubemap();
    assets.conv_lambert.levels.resize(1);
    assets.conv_ggx = dummy_cubemap();

    vierkant::save_bundle_file(assets, path);
    ASSERT_TRUE(std::filesystem::exists(path));

    auto loaded = vierkant::load_environment_bundle_file(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->skybox.format, VK_FORMAT_R16G16B16A16_SFLOAT);
    EXPECT_EQ(loaded->skybox.size, 2);
    EXPECT_EQ(loaded->skybox.levels, assets.skybox.levels);
    EXPECT_EQ(loaded->conv_lambert.levels.size(), 1);
    EXPECT_EQ(loaded->conv_ggx.levels, assets.conv_ggx.levels);
}

TEST(Bundle, texture_filename_covers_key_and_params)
{
    const std::filesystem::path key = "textures/wood/albedo.png";

    const auto base = vierkant::texture_bundle_filename(key, true, vierkant::bcn::BC7);
    EXPECT_TRUE(base.starts_with("albedo.png_"));
    EXPECT_TRUE(base.ends_with(".4kt"));

    // deterministic
    EXPECT_EQ(base, vierkant::texture_bundle_filename(key, true, vierkant::bcn::BC7));

    // compression-flag and mode both participate
    EXPECT_NE(base, vierkant::texture_bundle_filename(key, false, vierkant::bcn::BC7));
    EXPECT_NE(base, vierkant::texture_bundle_filename(key, true, vierkant::bcn::BC5));

    // same filename in a different directory must not collide
    EXPECT_NE(base, vierkant::texture_bundle_filename("textures/metal/albedo.png", true, vierkant::bcn::BC7));
}

TEST(Bundle, environment_filename_covers_key_and_params)
{
    const std::filesystem::path key = "env/sky.hdr";

    const auto base = vierkant::environment_bundle_filename(key, VK_FORMAT_R16G16B16A16_SFLOAT, 128);
    EXPECT_TRUE(base.starts_with("sky.hdr_"));
    EXPECT_TRUE(base.ends_with(".4ke"));

    EXPECT_EQ(base, vierkant::environment_bundle_filename(key, VK_FORMAT_R16G16B16A16_SFLOAT, 128));

    // hdr-format and convolution-size both participate
    EXPECT_NE(base, vierkant::environment_bundle_filename(key, VK_FORMAT_B10G11R11_UFLOAT_PACK32, 128));
    EXPECT_NE(base, vierkant::environment_bundle_filename(key, VK_FORMAT_R16G16B16A16_SFLOAT, 64));

    EXPECT_NE(base, vierkant::environment_bundle_filename("other/sky.hdr", VK_FORMAT_R16G16B16A16_SFLOAT, 128));
}
