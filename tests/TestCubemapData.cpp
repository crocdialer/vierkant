#include "test_context.hpp"

#include <vierkant/cubemap_data.hpp>
#include <vierkant/cubemap_utils.hpp>

namespace
{

constexpr uint32_t num_faces = 6;

//! a cubemap with a distinct byte-pattern per face and level, so misplaced regions are visible
vierkant::cubemap_data_t dummy_cubemap(uint32_t size, uint32_t num_levels, VkFormat format)
{
    vierkant::cubemap_data_t data = {};
    data.format = format;
    data.size = size;
    data.levels.resize(num_levels);

    for(uint32_t lvl = 0; lvl < num_levels; ++lvl)
    {
        const uint32_t level_size = std::max<uint32_t>(size >> lvl, 1);
        const size_t face_bytes = vierkant::num_bytes(format) * level_size * level_size;
        auto &blob = data.levels[lvl];
        blob.resize(num_faces * face_bytes);

        for(uint32_t face = 0; face < num_faces; ++face)
        {
            std::fill(blob.begin() + face * face_bytes, blob.begin() + (face + 1) * face_bytes,
                      static_cast<uint8_t>(1 + lvl * num_faces + face));
        }
    }
    return data;
}

}// namespace

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(CubemapData, upload_download_roundtrip)
{
    vulkan_test_context_t test_context;

    // 8x8 base with a full mip-chain, so per-level extents and per-face offsets are exercised
    const auto data = dummy_cubemap(8, 4, VK_FORMAT_R8G8B8A8_UNORM);

    auto cubemap = vierkant::upload_cubemap(test_context.device, data, test_context.device->queue());
    ASSERT_TRUE(cubemap);
    EXPECT_EQ(cubemap->format().num_layers, num_faces);
    EXPECT_EQ(cubemap->num_mip_levels(), data.levels.size());
    EXPECT_EQ(cubemap->image_layout(), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);

    auto downloaded = vierkant::download_cubemap(cubemap, test_context.device->queue());
    EXPECT_EQ(downloaded.format, data.format);
    EXPECT_EQ(downloaded.size, data.size);
    ASSERT_EQ(downloaded.levels.size(), data.levels.size());

    for(uint32_t lvl = 0; lvl < data.levels.size(); ++lvl) { EXPECT_EQ(downloaded.levels[lvl], data.levels[lvl]); }

    // the download must hand the image back in the layout it borrowed it in
    EXPECT_EQ(cubemap->image_layout(), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
}

TEST(CubemapData, single_level)
{
    vulkan_test_context_t test_context;

    // a lambert-convolution has no mip-chain
    const auto data = dummy_cubemap(16, 1, VK_FORMAT_R16G16B16A16_SFLOAT);

    auto cubemap = vierkant::upload_cubemap(test_context.device, data, test_context.device->queue());
    ASSERT_TRUE(cubemap);
    EXPECT_EQ(cubemap->num_mip_levels(), 1);

    auto downloaded = vierkant::download_cubemap(cubemap, test_context.device->queue());
    ASSERT_EQ(downloaded.levels.size(), 1);
    EXPECT_EQ(downloaded.levels[0], data.levels[0]);
}

TEST(CubemapData, empty_input)
{
    vulkan_test_context_t test_context;
    EXPECT_EQ(vierkant::upload_cubemap(test_context.device, {}, test_context.device->queue()), nullptr);
    EXPECT_TRUE(vierkant::download_cubemap(nullptr, test_context.device->queue()).levels.empty());
}

TEST(CubemapData, convolution_roundtrip)
{
    vulkan_test_context_t test_context;

    // the actual production path: a baked environment must survive a cache round-trip
    auto skybox = vierkant::cubemap_neutral_environment(test_context.device, 32, test_context.device->queue(), true,
                                                        VK_FORMAT_R16G16B16A16_SFLOAT);
    ASSERT_TRUE(skybox);

    auto conv_lambert = vierkant::create_convolution_lambert(test_context.device, skybox, 16,
                                                             VK_FORMAT_R16G16B16A16_SFLOAT,
                                                             test_context.device->queue());
    ASSERT_TRUE(conv_lambert);

    auto baked = vierkant::download_cubemap(conv_lambert, test_context.device->queue());
    ASSERT_FALSE(baked.levels.empty());

    auto restored = vierkant::upload_cubemap(test_context.device, baked, test_context.device->queue());
    ASSERT_TRUE(restored);
    EXPECT_EQ(vierkant::download_cubemap(restored, test_context.device->queue()).levels, baked.levels);
}
