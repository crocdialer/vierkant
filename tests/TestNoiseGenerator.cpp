#include "test_context.hpp"
#include "vierkant/NoiseGenerator.hpp"

//! generates once and reads all texels back, expects a 32-bit float color-format
static std::vector<float> generate_and_read(vulkan_test_context_t &test_context,
                                            const vierkant::NoiseGenerator::create_info_t &create_info,
                                            glm::vec3 scale, float alpha = 0.f)
{
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);
    const size_t num_bytes = vierkant::num_bytes(create_info.color_format) * create_info.size.width *
                             create_info.size.height * create_info.size.depth;
    auto host_buf = vierkant::Buffer::create(test_context.device, nullptr, num_bytes,
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    vierkant::CommandBuffer cmd_buffer(test_context.device, test_context.device->command_pool_transient());
    cmd_buffer.begin();
    auto img = generator->generate(cmd_buffer.handle(), scale, glm::vec3(0.f), alpha);
    img->copy_to(host_buf, cmd_buffer.handle());
    cmd_buffer.submit(test_context.device->queue(), true);

    const auto *texels = static_cast<const float *>(host_buf->map());
    return {texels, texels + num_bytes / sizeof(float)};
}

//! largest value-step between neighbours along an axis, inside the image and across the wrap
struct steps_t
{
    float interior = 0.f;
    float seam = 0.f;
};

static steps_t neighbour_steps(const std::vector<float> &texels, glm::uvec3 size, uint32_t num_channels, int axis)
{
    auto value = [&](glm::uvec3 p) { return texels[((p.z * size.y + p.y) * size.x + p.x) * num_channels]; };
    steps_t ret;

    for(uint32_t z = 0; z < size.z; ++z)
    {
        for(uint32_t y = 0; y < size.y; ++y)
        {
            for(uint32_t x = 0; x < size.x; ++x)
            {
                glm::uvec3 p = {x, y, z}, q = p;
                q[axis] = (p[axis] + 1) % size[axis];
                float step = std::abs(value(q) - value(p));
                float &max_step = q[axis] == 0 ? ret.seam : ret.interior;
                max_step = std::max(max_step, step);
            }
        }
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, create)
{
    vulkan_test_context_t test_context;

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = {256, 256, 1};
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);
    EXPECT_NE(generator, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, generate)
{
    vulkan_test_context_t test_context;

    constexpr VkExtent3D size = {256, 256, 1};
    constexpr VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = size;
    create_info.color_format = color_format;
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);
    ASSERT_NE(generator, nullptr);

    auto command_pool = vierkant::create_command_pool(test_context.device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    vierkant::CommandBuffer cmd_buffer(test_context.device, command_pool.get());
    cmd_buffer.begin();
    auto result = generator->generate(cmd_buffer.handle(), glm::vec3(1.f), glm::vec3(0.f));
    cmd_buffer.submit(test_context.device->queue(), true);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->width(), size.width);
    EXPECT_EQ(result->height(), size.height);
    EXPECT_EQ(result->format().format, color_format);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, generate_repeated)
{
    vulkan_test_context_t test_context;

    constexpr VkExtent3D size = {128, 128, 1};

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = size;
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);
    ASSERT_NE(generator, nullptr);

    auto command_pool = vierkant::create_command_pool(test_context.device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    constexpr float scale_step = 0.5f;
    constexpr glm::vec3 seed_step = {0.1f, 0.2f, 0.3f};
    constexpr int num_calls = 5;

    for(int i = 0; i < num_calls; ++i)
    {
        float s = static_cast<float>(i + 1) * scale_step;
        vierkant::CommandBuffer cmd_buffer(test_context.device, command_pool.get());
        cmd_buffer.begin();
        auto result = generator->generate(cmd_buffer.handle(), glm::vec3(s), static_cast<float>(i) * seed_step);
        cmd_buffer.submit(test_context.device->queue(), true);
        EXPECT_NE(result, nullptr);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, tiling_2d)
{
    vulkan_test_context_t test_context;

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = {64, 64, 1};
    create_info.color_format = VK_FORMAT_R32_SFLOAT;
    auto texels = generate_and_read(test_context, create_info, {4.f, 5.f, 0.f});

    // value in [0, 1], and actually noise, not a constant
    double sum = 0.0, sum_sq = 0.0;
    for(float v: texels)
    {
        EXPECT_GE(v, 0.f);
        EXPECT_LE(v, 1.f);
        sum += v;
        sum_sq += v * v;
    }
    double mean = sum / texels.size();
    EXPECT_NEAR(mean, 0.5, 0.1);
    EXPECT_GT(std::sqrt(sum_sq / texels.size() - mean * mean), 0.05);

    // a broken wrap shows as a seam-step far above the steps inside
    glm::uvec3 size = {64, 64, 1};
    for(int axis = 0; axis < 2; ++axis)
    {
        auto steps = neighbour_steps(texels, size, 1, axis);
        EXPECT_LE(steps.seam, 1.5f * steps.interior) << "axis " << axis;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, non_integer_scale)
{
    vulkan_test_context_t test_context;

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = {64, 64, 1};
    create_info.color_format = VK_FORMAT_R32_SFLOAT;
    glm::uvec3 size = {64, 64, 1};

    // x: 4.5 cannot tile, so the wrap shows a seam. y: 4 still tiles
    auto texels = generate_and_read(test_context, create_info, {4.5f, 4.f, 0.f});
    auto steps_x = neighbour_steps(texels, size, 1, 0);
    auto steps_y = neighbour_steps(texels, size, 1, 1);
    EXPECT_GT(steps_x.seam, 1.5f * steps_x.interior);
    EXPECT_LE(steps_y.seam, 1.5f * steps_y.interior);

    // no wrap along x, so no seams inside the image either
    auto ref = neighbour_steps(generate_and_read(test_context, create_info, {4.f, 4.f, 0.f}), size, 1, 0);
    EXPECT_LT(steps_x.interior, 2.f * ref.interior);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, tiling_3d)
{
    vulkan_test_context_t test_context;

    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = {32, 32, 32};
    create_info.color_format = VK_FORMAT_R32_SFLOAT;
    auto texels = generate_and_read(test_context, create_info, {3.f, 4.f, 5.f});

    glm::uvec3 size = {32, 32, 32};
    for(int axis = 0; axis < 3; ++axis)
    {
        auto steps = neighbour_steps(texels, size, 1, axis);

        // varies along every axis, z included
        EXPECT_GT(steps.interior, 0.01f) << "axis " << axis;
        EXPECT_LE(steps.seam, 1.5f * steps.interior) << "axis " << axis;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, gradient)
{
    vulkan_test_context_t test_context;

    constexpr uint32_t N = 128;
    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = {N, N, 1};
    create_info.color_format = VK_FORMAT_R32G32B32A32_SFLOAT;

    // off: gba stay zero
    {
        auto texels = generate_and_read(test_context, create_info, {3.f, 5.f, 0.f});
        float max_gba = 0.f;
        for(size_t i = 0; i < texels.size(); i += 4)
        {
            max_gba = std::max({max_gba, std::abs(texels[i + 1]), std::abs(texels[i + 2]), std::abs(texels[i + 3])});
        }
        EXPECT_EQ(max_gba, 0.f);
    }

    // on: gb match central differences of r per unit texture-coordinate. alpha exercises the rotation
    create_info.gradient = true;
    auto texels = generate_and_read(test_context, create_info, {3.f, 5.f, 0.f}, 0.7f);
    auto texel = [&](uint32_t x, uint32_t y, uint32_t c) { return texels[((y % N) * N + (x % N)) * 4 + c]; };

    float max_err = 0.f, max_grad = 0.f;
    for(uint32_t y = 0; y < N; ++y)
    {
        for(uint32_t x = 0; x < N; ++x)
        {
            float dx = (texel(x + 1, y, 0) - texel(x + N - 1, y, 0)) * 0.5f * N;
            float dy = (texel(x, y + 1, 0) - texel(x, y + N - 1, 0)) * 0.5f * N;
            max_err = std::max({max_err, std::abs(dx - texel(x, y, 1)), std::abs(dy - texel(x, y, 2))});
            max_grad = std::max({max_grad, std::abs(texel(x, y, 1)), std::abs(texel(x, y, 2))});
        }
    }
    EXPECT_GT(max_grad, 0.5f);
    EXPECT_LT(max_err, 0.02f * max_grad) << "max_err " << max_err << ", max_grad " << max_grad;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TEST(NoiseGenerator, mipmap)
{
    vulkan_test_context_t test_context;

    constexpr uint32_t N = 16;
    vierkant::NoiseGenerator::create_info_t create_info = {};
    create_info.size = {N, N, N};
    create_info.color_format = VK_FORMAT_R32_SFLOAT;
    create_info.use_mipmap = true;
    auto generator = vierkant::NoiseGenerator::create(test_context.device, create_info);

    constexpr size_t num_bytes_0 = sizeof(float) * N * N * N, num_bytes_1 = num_bytes_0 / 8;
    auto buf_0 = vierkant::Buffer::create(test_context.device, nullptr, num_bytes_0, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VMA_MEMORY_USAGE_CPU_ONLY);
    auto buf_1 = vierkant::Buffer::create(test_context.device, nullptr, num_bytes_1, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VMA_MEMORY_USAGE_CPU_ONLY);

    vierkant::CommandBuffer cmd_buffer(test_context.device, test_context.device->command_pool_transient());
    cmd_buffer.begin();
    auto img = generator->generate(cmd_buffer.handle(), {2.f, 3.f, 4.f}, glm::vec3(0.f));
    img->copy_to(buf_0, cmd_buffer.handle());
    img->copy_to(buf_1, cmd_buffer.handle(), 0, {0, 0, 0}, {N / 2, N / 2, N / 2}, 0, 1);
    cmd_buffer.submit(test_context.device->queue(), true);
    EXPECT_EQ(img->num_mip_levels(), 5);

    // a 2:1 linear blit averages each 2x2x2 block of level 0
    const auto *level_0 = static_cast<const float *>(buf_0->map());
    const auto *level_1 = static_cast<const float *>(buf_1->map());
    constexpr uint32_t M = N / 2;
    float max_err = 0.f;

    for(uint32_t z = 0; z < M; ++z)
    {
        for(uint32_t y = 0; y < M; ++y)
        {
            for(uint32_t x = 0; x < M; ++x)
            {
                float sum = 0.f;
                for(uint32_t i = 0; i < 8; ++i)
                {
                    uint32_t sx = 2 * x + (i & 1), sy = 2 * y + ((i >> 1) & 1), sz = 2 * z + (i >> 2);
                    sum += level_0[(sz * N + sy) * N + sx];
                }
                max_err = std::max(max_err, std::abs(level_1[(z * M + y) * M + x] - sum / 8.f));
            }
        }
    }
    EXPECT_LT(max_err, 1e-4f);
}
