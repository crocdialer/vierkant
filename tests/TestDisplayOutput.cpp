#include "test_context.hpp"
#include <vierkant/DisplayOutput.hpp>

#include <cmath>

namespace
{

// SMPTE ST 2084 inverse EOTF, normalized linear (1.0 <-> 10000 cd/m²) -> PQ code-value
float pq_encode(float y)
{
    constexpr float c1 = 3424.f / 4096.f, c2 = 2413.f / 128.f, c3 = 2392.f / 128.f;
    constexpr float m1 = 2610.f / 16384.f, m2 = 2523.f / 32.f;
    float ym = std::pow(std::max(y, 0.f), m1);
    return std::pow((c1 + c2 * ym) / (1.f + c3 * ym), m2);
}

// ITU-R BT.2087
glm::vec3 bt709_to_bt2020(const glm::vec3 &c)
{
    return {0.6274f * c.x + 0.3293f * c.y + 0.0433f * c.z, 0.0691f * c.x + 0.9195f * c.y + 0.0114f * c.z,
            0.0164f * c.x + 0.0880f * c.y + 0.8956f * c.z};
}

glm::vec4 reference(const glm::vec4 &scene, const glm::vec4 &ui, const vierkant::DisplayOutput::settings_t &s,
                    vierkant::DisplayOutput::Encoding encoding)
{
    glm::vec3 rgb(scene);
    glm::vec3 sdr = glm::pow(1.f - glm::exp(-rgb * s.exposure), glm::vec3(1.f / s.gamma));
    sdr = sdr * (1.f - ui.w) + glm::vec3(ui);

    if(encoding == vierkant::DisplayOutput::Encoding::HDR10)
    {
        float k = s.peak_nits / s.paper_white_nits;
        glm::vec3 color = k * (1.f - glm::exp(-rgb * s.exposure / k));

        // under ui: the linearized SDR result
        if(ui.w > 0.f) { color = glm::pow(sdr, glm::vec3(s.gamma)); }
        glm::vec3 nits = bt709_to_bt2020(color) * s.paper_white_nits;
        return {pq_encode(nits.x / 10000.f), pq_encode(nits.y / 10000.f), pq_encode(nits.z / 10000.f), 1.f};
    }
    return {sdr, scene.w * (1.f - ui.w) + ui.w};
}

//! run a DisplayOutput over a row of texels, return the float results
std::vector<glm::vec4> run_display_output(const vierkant::DevicePtr &device, const vierkant::DisplayOutput &display,
                                          const std::vector<glm::vec4> &scene, const std::vector<glm::vec4> &ui,
                                          vierkant::DisplayOutput::Encoding encoding)
{
    auto width = static_cast<uint32_t>(scene.size());

    vierkant::Image::Format img_fmt;
    img_fmt.extent = {width, 1, 1};
    img_fmt.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    auto scene_img = vierkant::Image::create(device, scene.data(), img_fmt);
    vierkant::ImagePtr ui_img = ui.empty() ? nullptr : vierkant::Image::create(device, ui.data(), img_fmt);

    vierkant::Framebuffer::create_info_t framebuffer_info = {};
    framebuffer_info.size = {width, 1, 1};
    framebuffer_info.color_attachment_format.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    framebuffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    vierkant::Framebuffer framebuffer(device, framebuffer_info);

    vierkant::Rasterizer::create_info_t rasterizer_info = {};
    rasterizer_info.num_frames_in_flight = 1;
    rasterizer_info.viewport = {0.f, 0.f, static_cast<float>(width), 1.f, 0.f, 1.f};
    vierkant::Rasterizer rasterizer(device, rasterizer_info);

    display.draw(rasterizer, scene_img, ui_img, encoding);
    framebuffer.submit({rasterizer.render(framebuffer)}, device->queue());
    framebuffer.wait_fence();

    auto host_buffer = vierkant::Buffer::create(device, nullptr, width * sizeof(glm::vec4),
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    framebuffer.color_attachment()->copy_to(host_buffer);
    auto ptr = static_cast<const glm::vec4 *>(host_buffer->map());
    return {ptr, ptr + width};
}

const std::vector<glm::vec4> g_scene = {{0.f, 0.f, 0.f, 1.f},     {0.05f, 0.1f, 0.2f, 1.f}, {0.25f, 0.5f, 1.f, 0.5f},
                                        {1.f, 1.f, 1.f, 1.f},     {4.f, 2.f, 1.f, 1.f},     {16.f, 0.f, 0.f, 1.f},
                                        {100.f, 100.f, 100.f, 0.f}, {1.f, 0.f, 0.f, 1.f}};

// premultiplied, sRGB-encoded
const std::vector<glm::vec4> g_ui = {{0.f, 0.f, 0.f, 0.f}, {0.5f, 0.5f, 0.5f, 1.f}, {0.2f, 0.f, 0.f, 0.25f},
                                     {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 0.f},    {0.f, 0.3f, 0.f, 0.5f},
                                     {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}};

void check(const std::vector<glm::vec4> &result, const std::vector<glm::vec4> &scene,
           const std::vector<glm::vec4> &ui, const vierkant::DisplayOutput::settings_t &settings,
           vierkant::DisplayOutput::Encoding encoding)
{
    ASSERT_EQ(result.size(), scene.size());
    for(uint32_t i = 0; i < result.size(); ++i)
    {
        auto expected = reference(scene[i], ui.empty() ? glm::vec4(0.f) : ui[i], settings, encoding);
        for(uint32_t c = 0; c < 4; ++c) { EXPECT_NEAR(result[i][c], expected[c], 1e-4f) << "texel " << i << " c " << c; }
    }
}

}// namespace

TEST(DisplayOutput, SDR)
{
    vulkan_test_context_t test_context;
    vierkant::DisplayOutput display(test_context.device);
    display.settings.exposure = 1.5f;

    auto encoding = vierkant::DisplayOutput::Encoding::SDR;
    check(run_display_output(test_context.device, display, g_scene, {}, encoding), g_scene, {}, display.settings,
          encoding);
    check(run_display_output(test_context.device, display, g_scene, g_ui, encoding), g_scene, g_ui, display.settings,
          encoding);
}

TEST(DisplayOutput, HDR10)
{
    vulkan_test_context_t test_context;
    vierkant::DisplayOutput display(test_context.device);
    display.settings.exposure = 1.5f;

    auto encoding = vierkant::DisplayOutput::Encoding::HDR10;
    check(run_display_output(test_context.device, display, g_scene, {}, encoding), g_scene, {}, display.settings,
          encoding);
    auto result = run_display_output(test_context.device, display, g_scene, g_ui, encoding);
    check(result, g_scene, g_ui, display.settings, encoding);

    // opaque white ui is paper-white: 203 cd/m² -> PQ 0.5807 (independent of the tone-mapping)
    for(uint32_t c = 0; c < 3; ++c) { EXPECT_NEAR(result[3][c], 0.5807f, 1e-4f); }
}
