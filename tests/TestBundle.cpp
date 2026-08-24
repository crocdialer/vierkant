#include <gtest/gtest.h>

#include <random>

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
