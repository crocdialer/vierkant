// Compute users through descriptor-sets and through a descriptor-heap must produce identical bits.

#include "test_context.hpp"
#include <glm/gtc/packing.hpp>
#include <test_shaders/shaders_slang.hpp>
#include <vierkant/Compute.hpp>
#include <vierkant/NoiseGenerator.hpp>
#include <vierkant/cubemap_data.hpp>
#include <vierkant/gpu_culling.hpp>

namespace
{

bool descriptor_heap_supported()
{
    vierkant::Instance instance(vierkant::Instance::create_info_t{});
    return !instance.physical_devices().empty() &&
           vierkant::check_device_extension_support(instance.physical_devices()[0],
                                                    vierkant::DescriptorHeap::required_extensions());
}

//! runs fn(device, pipeline_cache) for descriptor-sets [0] and descriptor-heap [1], each on its own device.
//! a fresh device per path keeps one path's output out of memory the other path reads back.
template<typename Fn>
auto per_path(Fn fn)
{
    std::array<decltype(fn(vierkant::DevicePtr{}, vierkant::PipelineCachePtr{})), 2> ret;
    for(uint32_t path = 0; path < 2; ++path)
    {
        vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
        const auto &device = test_context.device;
        auto cache = path ? vierkant::PipelineCache::create(device, vierkant::DescriptorHeap::create({device}))
                          : vierkant::PipelineCache::create(device);
        ret[path] = fn(device, cache);
    }
    return ret;
}

std::vector<uint8_t> read_image(const vierkant::DevicePtr &device, const vierkant::ImagePtr &img, uint32_t level)
{
    VkExtent3D extent = {std::max(1u, img->width() >> level), std::max(1u, img->height() >> level),
                         std::max(1u, img->depth() >> level)};
    size_t num_bytes = vierkant::num_bytes(img->format().format) * extent.width * extent.height * extent.depth;
    auto host_buf = vierkant::Buffer::create(device, nullptr, num_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VMA_MEMORY_USAGE_CPU_ONLY);
    vierkant::CommandBuffer cmd(device, device->command_pool_transient());
    cmd.begin();
    img->copy_to(host_buf, cmd.handle(), 0, {0, 0, 0}, extent, 0, level);
    cmd.submit(device->queue(), true);
    const auto *data = static_cast<const uint8_t *>(host_buf->map());
    return {data, data + num_bytes};
}

bool all_zero(const std::vector<uint8_t> &bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b == 0; });
}

}// namespace

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ComputeHeap, noise)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    // 2d, 2d with gradient, 3d
    std::vector<vierkant::NoiseGenerator::create_info_t> variants(3);
    variants[0].size = {64, 32, 1};
    variants[0].color_format = VK_FORMAT_R32_SFLOAT;
    variants[1].size = {32, 32, 1};
    variants[1].color_format = VK_FORMAT_R32G32B32A32_SFLOAT;
    variants[1].gradient = true;
    variants[2].size = {16, 16, 16};
    variants[2].color_format = VK_FORMAT_R32_SFLOAT;

    auto results = per_path([&](const vierkant::DevicePtr &device, const vierkant::PipelineCachePtr &cache) {
        std::vector<std::vector<uint8_t>> ret;
        for(auto create_info: variants)
        {
            create_info.pipeline_cache = cache;
            auto generator = vierkant::NoiseGenerator::create(device, create_info);

            vierkant::CommandBuffer cmd(device, device->command_pool_transient());
            cmd.begin();
            auto img = generator->generate(cmd.handle(), glm::vec3(4.f), glm::vec3(0.5f, 1.5f, 2.5f), 0.25f);
            cmd.submit(device->queue(), true);
            ret.push_back(read_image(device, img, 0));
        }
        return ret;
    });

    for(uint32_t i = 0; i < variants.size(); ++i)
    {
        EXPECT_FALSE(all_zero(results[1][i])) << "variant " << i;
        EXPECT_TRUE(results[0][i] == results[1][i]) << "variant " << i;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ComputeHeap, depth_pyramid)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    // depth-values without structure, so every reduction-step matters
    constexpr uint32_t width = 100, height = 60;
    std::vector<float> depths(width * height);
    for(uint32_t i = 0; i < depths.size(); ++i) { depths[i] = float((i * 7919u) % 1000u) / 1000.f; }

    auto results = per_path([&](const vierkant::DevicePtr &device, const vierkant::PipelineCachePtr &cache) {
        vierkant::Image::Format depth_fmt = {};
        depth_fmt.extent = {width, height, 1};
        depth_fmt.format = VK_FORMAT_R32_SFLOAT;
        depth_fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        auto depth_map = vierkant::Image::create(device, depths.data(), depth_fmt);

        auto context = vierkant::create_gpu_cull_context(device, {width, height}, cache);
        vierkant::create_depth_pyramid_params_t params = {};
        params.depth_map = depth_map;
        params.queue = device->queue();
        params.query_pool = vierkant::create_query_pool(device, 2, VK_QUERY_TYPE_TIMESTAMP);
        params.query_index_end = 1;
        auto pyramid = vierkant::create_depth_pyramid(context, params);
        vkQueueWaitIdle(device->queue());

        std::vector<std::vector<uint8_t>> levels;
        for(uint32_t lvl = 0; lvl < pyramid->num_mip_levels(); ++lvl)
        {
            levels.push_back(read_image(device, pyramid, lvl));
        }
        return levels;
    });

    ASSERT_GT(results[0].size(), 2);
    ASSERT_EQ(results[0].size(), results[1].size());
    for(uint32_t lvl = 0; lvl < results[0].size(); ++lvl)
    {
        EXPECT_FALSE(all_zero(results[1][lvl])) << "level " << lvl;
        EXPECT_TRUE(results[0][lvl] == results[1][lvl]) << "level " << lvl;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ComputeHeap, bc6h_compress)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }

    // several mip-levels: one computable per level, dispatched together
    constexpr uint32_t size = 16;
    vierkant::cubemap_data_t source = {};
    source.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    source.size = size;
    for(uint32_t lvl = 0; lvl < 2; ++lvl)
    {
        uint32_t level_size = size >> lvl;
        auto &level = source.levels.emplace_back(vierkant::cubemap_level_num_bytes(source.format, level_size));
        auto *texels = reinterpret_cast<uint16_t *>(level.data());
        for(size_t i = 0; i < level.size() / sizeof(uint16_t); ++i)
        {
            texels[i] = glm::packHalf1x16(0.25f + float((i * 37u) % 101u) / 16.f);
        }
    }

    auto results = per_path([&](const vierkant::DevicePtr &device, const vierkant::PipelineCachePtr &cache) {
        auto src_image = vierkant::upload_cubemap(device, source, device->queue());
        EXPECT_TRUE(src_image && src_image->num_mip_levels() > 1);
        return src_image ? vierkant::compress_cubemap(src_image, device->queue(), cache) : vierkant::cubemap_data_t{};
    });

    ASSERT_GT(results[0].levels.size(), 1);
    ASSERT_EQ(results[1].levels.size(), results[0].levels.size());
    for(uint32_t lvl = 0; lvl < results[0].levels.size(); ++lvl)
    {
        EXPECT_FALSE(all_zero(results[1].levels[lvl])) << "level " << lvl;
        EXPECT_TRUE(results[0].levels[lvl] == results[1].levels[lvl]) << "level " << lvl;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ComputeHeap, arena_growth)
{
    if(!descriptor_heap_supported()) { GTEST_SKIP() << "VK_EXT_descriptor_heap not supported"; }
    vulkan_test_context_t test_context(vierkant::DescriptorHeap::required_extensions());
    const auto &device = test_context.device;

    // many small dispatches in one frame-slot, more than the initial arenas hold.
    // each writes its index through a storage-buffer at its own offset.
    constexpr uint32_t num_dispatches = 1000;
    const auto ssbo_alignment = device->properties().core.limits.minStorageBufferOffsetAlignment;
    auto out = vierkant::Buffer::create(device, nullptr, num_dispatches * ssbo_alignment,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
    memset(out->map(), 0, out->num_bytes());
    out->unmap();

    vierkant::Compute::create_info_t compute_info = {};
    compute_info.pipeline_cache = vierkant::PipelineCache::create(device, vierkant::DescriptorHeap::create({device}));
    compute_info.num_frames_in_flight = 2;
    vierkant::Compute compute(device, compute_info);

    VkPushConstantRange push_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)};

    // three rounds over two frame-slots: the third reuses the first slot after a reset
    for(uint32_t round = 0; round < 3; ++round)
    {
        std::vector<vierkant::Compute::computable_t> computables(num_dispatches);
        for(uint32_t i = 0; i < num_dispatches; ++i)
        {
            auto &c = computables[i];
            c.pipeline_info.shader_stage =
                    vierkant::create_shader_module(vierkant::slang_shaders::shaders::compute_heap_test_slang);
            c.pipeline_info.push_constant_ranges = {push_range};
            c.extent = {1, 1, 1};
            c.descriptors[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT};
            c.descriptors[0].buffers = {out};
            c.descriptors[0].buffer_ranges = {{i * ssbo_alignment, sizeof(uint32_t)}};
            uint32_t value = round * num_dispatches + i + 1;
            c.push_constants.resize(sizeof(uint32_t));
            memcpy(c.push_constants.data(), &value, sizeof(uint32_t));
        }
        vierkant::CommandBuffer cmd(device, device->command_pool_transient());
        cmd.begin();
        compute.dispatch(computables, cmd.handle());
        cmd.submit(device->queue(), true);

        const auto *data = static_cast<const uint8_t *>(out->map());
        uint32_t num_wrong = 0;
        for(uint32_t i = 0; i < num_dispatches; ++i)
        {
            uint32_t value;
            memcpy(&value, data + i * ssbo_alignment, sizeof(uint32_t));
            num_wrong += value != round * num_dispatches + i + 1;
        }
        out->unmap();
        EXPECT_EQ(num_wrong, 0) << "round " << round;
    }
}
