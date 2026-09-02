#include "test_context.hpp"

#include <glm/gtc/packing.hpp>

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

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

//! a smooth HDR gradient per face, in RGBA16F. exactly the content BC6H should nail.
vierkant::cubemap_data_t hdr_gradient_cubemap(uint32_t size)
{
    vierkant::cubemap_data_t data = {};
    data.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    data.size = size;
    data.levels.resize(1);
    data.levels[0].resize(vierkant::cubemap_level_num_bytes(data.format, size));

    auto *texels = reinterpret_cast<uint16_t *>(data.levels[0].data());

    for(uint32_t face = 0; face < num_faces; ++face)
    {
        for(uint32_t y = 0; y < size; ++y)
        {
            for(uint32_t x = 0; x < size; ++x)
            {
                float u = static_cast<float>(x) / static_cast<float>(size - 1);
                float v = static_cast<float>(y) / static_cast<float>(size - 1);

                // an HDR range well beyond [0..1], the point of using a float format at all
                glm::vec3 c = {0.25f + 8.0f * u, 0.5f + 4.0f * v, 1.0f + 2.0f * (u * v) + 0.5f * float(face)};

                size_t idx = 4 * ((face * size + y) * size + x);
                texels[idx + 0] = static_cast<uint16_t>(glm::packHalf1x16(c.x));
                texels[idx + 1] = static_cast<uint16_t>(glm::packHalf1x16(c.y));
                texels[idx + 2] = static_cast<uint16_t>(glm::packHalf1x16(c.z));
                texels[idx + 3] = static_cast<uint16_t>(glm::packHalf1x16(1.0f));
            }
        }
    }
    return data;
}

//! decode a BC6H cubemap by blitting it into an RGBA16F one - the hardware decoder is the reference
vierkant::ImagePtr decode_via_blit(const vierkant::DevicePtr &device, const vierkant::ImagePtr &compressed,
                                   VkQueue queue)
{
    vierkant::Image::Format fmt = {};
    fmt.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    fmt.extent = compressed->format().extent;
    fmt.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    fmt.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    fmt.num_layers = num_faces;
    fmt.use_mipmap = false;
    fmt.initial_layout_transition = false;
    auto decoded = vierkant::Image::create(device, fmt);

    auto pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                              VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto cmd_buf = vierkant::CommandBuffer(device, pool.get());
    cmd_buf.begin();
    compressed->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmd_buf.handle());
    decoded->transition_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd_buf.handle());

    VkImageBlit region = {};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, num_faces};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, num_faces};
    region.srcOffsets[1] = {static_cast<int32_t>(fmt.extent.width), static_cast<int32_t>(fmt.extent.height), 1};
    region.dstOffsets[1] = region.srcOffsets[1];
    vkCmdBlitImage(cmd_buf.handle(), compressed->image(), compressed->image_layout(), decoded->image(),
                   decoded->image_layout(), 1, &region, VK_FILTER_NEAREST);

    decoded->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmd_buf.handle());
    compressed->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, cmd_buf.handle());
    cmd_buf.submit(queue, true);
    return decoded;
}

struct error_stats_t
{
    //! root-mean-squared-log-error, the metric the reference encoder reports quality in
    double rmsle = 0.0;

    //! largest per-channel error under the encoder's own metric. RMS averages outliers away, this
    //! does not, so a single mangled block shows up here.
    double max_msle_error = 0.0;

    //! largest per-channel *relative* error in octaves. the encoder does not optimize this - the
    //! log1p form above deliberately tolerates large relative error on near-black values.
    double max_relative_error = 0.0;

    size_t worst_texel = 0;
    uint32_t worst_channel = 0;
    double worst_src = 0.0, worst_dst = 0.0;
};

error_stats_t compare(const vierkant::cubemap_data_t &a, const vierkant::cubemap_data_t &b)
{
    const auto *ta = reinterpret_cast<const uint16_t *>(a.levels[0].data());
    const auto *tb = reinterpret_cast<const uint16_t *>(b.levels[0].data());
    const size_t num_texels = a.levels[0].size() / (4 * sizeof(uint16_t));

    error_stats_t ret = {};
    double sum = 0.0;

    for(size_t i = 0; i < num_texels; ++i)
    {
        for(uint32_t c = 0; c < 3; ++c)
        {
            double va = glm::unpackHalf1x16(ta[4 * i + c]);
            double vb = glm::unpackHalf1x16(tb[4 * i + c]);
            double delta = std::log2((vb + 1.0) / (va + 1.0));
            sum += delta * delta;

            if(std::abs(delta) > ret.max_msle_error)
            {
                ret.max_msle_error = std::abs(delta);
                ret.worst_texel = i;
                ret.worst_channel = c;
                ret.worst_src = va;
                ret.worst_dst = vb;
            }
            ret.max_relative_error = std::max(ret.max_relative_error, std::abs(std::log2(vb / va)));
        }
    }
    ret.rmsle = std::sqrt(sum / static_cast<double>(num_texels * 3));
    return ret;
}

}// namespace

TEST(CubemapData, bc6h_sizes)
{
    // uncompressed: 6 faces of size^2 texels
    EXPECT_EQ(vierkant::cubemap_level_num_bytes(VK_FORMAT_R16G16B16A16_SFLOAT, 8), 6 * 8 * 8 * 8);

    // BC6H: 6 faces of (size/4)^2 blocks, 16 byte each -> 1 byte per texel
    EXPECT_EQ(vierkant::cubemap_level_num_bytes(VK_FORMAT_BC6H_UFLOAT_BLOCK, 8), 6 * 2 * 2 * 16);

    // the mip-tail below the block-size still occupies one full block per face
    EXPECT_EQ(vierkant::cubemap_level_num_bytes(VK_FORMAT_BC6H_UFLOAT_BLOCK, 2), 6 * 1 * 1 * 16);
    EXPECT_EQ(vierkant::cubemap_level_num_bytes(VK_FORMAT_BC6H_UFLOAT_BLOCK, 1), 6 * 1 * 1 * 16);
}

//! flat HDR colour per face - BC6H should reproduce this near-exactly
vierkant::cubemap_data_t hdr_flat_cubemap(uint32_t size)
{
    vierkant::cubemap_data_t data = {};
    data.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    data.size = size;
    data.levels.resize(1);
    data.levels[0].resize(vierkant::cubemap_level_num_bytes(data.format, size));

    auto *texels = reinterpret_cast<uint16_t *>(data.levels[0].data());

    for(uint32_t face = 0; face < num_faces; ++face)
    {
        glm::vec3 c = {1.0f + float(face), 4.0f - 0.5f * float(face), 0.125f * float(face + 1)};

        for(uint32_t i = 0; i < size * size; ++i)
        {
            size_t idx = 4 * (face * size * size + i);
            texels[idx + 0] = static_cast<uint16_t>(glm::packHalf1x16(c.x));
            texels[idx + 1] = static_cast<uint16_t>(glm::packHalf1x16(c.y));
            texels[idx + 2] = static_cast<uint16_t>(glm::packHalf1x16(c.z));
            texels[idx + 3] = static_cast<uint16_t>(glm::packHalf1x16(1.0f));
        }
    }
    return data;
}

TEST(CubemapData, bc6h_flat_color)
{
    vulkan_test_context_t test_context;
    const auto queue = test_context.device->queue();
    constexpr uint32_t size = 32;

    // a block with a single colour has both endpoints on top of each other: if the port mangled the
    // endpoint bit-packing or the mode selection, this is where it shows up unambiguously
    const auto source = hdr_flat_cubemap(size);
    auto src_image = vierkant::upload_cubemap(test_context.device, source, queue);
    auto compressed = vierkant::compress_cubemap(src_image, queue);
    auto bc6h_image = vierkant::upload_cubemap(test_context.device, compressed, queue);
    ASSERT_TRUE(bc6h_image);

    auto decoded = vierkant::download_cubemap(decode_via_blit(test_context.device, bc6h_image, queue), queue);
    const auto error = compare(source, decoded);
    std::cout << "BC6H flat: rmsle " << error.rmsle << ", max relative " << error.max_relative_error
              << " octaves\n";

    // a flat block has both endpoints on the same colour, so its error is bounded by endpoint
    // precision alone. mode 11 keeps 10 bits of a half's bit-pattern, and that pattern spans ~30
    // octaves -> one step is 30/1024 = 0.0293 octaves. landing just inside that bound is proof the
    // endpoints and their bit-packing are right; a mangled block misses by orders of magnitude.
    EXPECT_LT(error.max_relative_error, 0.0293);
}

TEST(CubemapData, bc6h_compress)
{
    vulkan_test_context_t test_context;
    const auto queue = test_context.device->queue();
    constexpr uint32_t size = 64;

    const auto source = hdr_gradient_cubemap(size);
    auto src_image = vierkant::upload_cubemap(test_context.device, source, queue);
    ASSERT_TRUE(src_image);

    auto compressed = vierkant::compress_cubemap(src_image, queue);
    EXPECT_EQ(compressed.format, VK_FORMAT_BC6H_UFLOAT_BLOCK);
    EXPECT_EQ(compressed.size, size);
    ASSERT_EQ(compressed.levels.size(), 1);

    // 8x the smaller than the RGBA16F source
    EXPECT_EQ(compressed.levels[0].size(), vierkant::cubemap_level_num_bytes(VK_FORMAT_BC6H_UFLOAT_BLOCK, size));
    EXPECT_EQ(compressed.levels[0].size() * 8, source.levels[0].size());

    // an all-zero result would mean the dispatch never wrote anything
    EXPECT_NE(std::count(compressed.levels[0].begin(), compressed.levels[0].end(), uint8_t(0)),
              static_cast<long>(compressed.levels[0].size()));

    auto bc6h_image = vierkant::upload_cubemap(test_context.device, compressed, queue);
    ASSERT_TRUE(bc6h_image);
    EXPECT_EQ(bc6h_image->format().format, VK_FORMAT_BC6H_UFLOAT_BLOCK);

    // let the hardware decoder judge the encoding
    auto decoded_image = decode_via_blit(test_context.device, bc6h_image, queue);
    auto decoded = vierkant::download_cubemap(decoded_image, queue);
    ASSERT_EQ(decoded.levels.size(), 1);
    ASSERT_EQ(decoded.levels[0].size(), source.levels[0].size());

    const auto error = compare(source, decoded);
    std::cout << "BC6H gradient: rmsle " << error.rmsle << ", max msle " << error.max_msle_error
              << ", max relative " << error.max_relative_error << " octaves\n";
    {
        const uint32_t face = static_cast<uint32_t>(error.worst_texel / (size * size));
        const size_t rem = error.worst_texel % (size * size);
        std::cout << "  worst: face " << face << " (" << rem % size << ", " << rem / size << ") channel "
                  << error.worst_channel << ": " << error.worst_src << " -> " << error.worst_dst << "\n";
    }

    // the reference encoder reports RMSLE 0.0066 - 0.033 across its five test scenes, so a result in
    // that band means the port reproduces the original's quality
    EXPECT_LT(error.rmsle, 0.033);

    // ...and no block may fall apart. this is a "still a valid encoding" guard, not a quality
    // bound: a mispacked block decodes orders of magnitude off, while the worst legitimate block
    // here sits around 0.15 - the darkest texels, where this encoder's log1p metric deliberately
    // stops caring (source 0.63 decodes to 0.47 in the first block-column of the ramp).
    EXPECT_LT(error.max_msle_error, 0.5);
}

TEST(CubemapData, bc6h_mip_chain)
{
    vulkan_test_context_t test_context;
    const auto queue = test_context.device->queue();

    // a mipped cubemap running past the 4x4 block-size, down to 1x1
    auto skybox = vierkant::cubemap_neutral_environment(test_context.device, 16, queue, true,
                                                        VK_FORMAT_R16G16B16A16_SFLOAT);
    ASSERT_TRUE(skybox);
    ASSERT_EQ(skybox->num_mip_levels(), 5);

    auto compressed = vierkant::compress_cubemap(skybox, queue);
    ASSERT_EQ(compressed.levels.size(), 5);

    for(uint32_t lvl = 0; lvl < compressed.levels.size(); ++lvl)
    {
        const uint32_t level_size = std::max<uint32_t>(compressed.size >> lvl, 1);
        EXPECT_EQ(compressed.levels[lvl].size(),
                  vierkant::cubemap_level_num_bytes(VK_FORMAT_BC6H_UFLOAT_BLOCK, level_size));
    }

    // the full chain has to survive a re-upload, mip-tail included
    auto bc6h_image = vierkant::upload_cubemap(test_context.device, compressed, queue);
    ASSERT_TRUE(bc6h_image);
    EXPECT_EQ(bc6h_image->num_mip_levels(), 5);
}
