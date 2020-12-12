//
// Created by crocdialer on 6/19/20.
//
#pragma once

#include "vierkant/culling.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/DrawContext.hpp"
#include "vierkant/SceneRenderer.hpp"
#include "vierkant/Bloom.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(PBRDeferred);

class PBRDeferred : public vierkant::SceneRenderer
{
public:

    enum G_BUFFER
    {
        G_BUFFER_ALBEDO = 0,
        G_BUFFER_NORMAL = 1,
        G_BUFFER_POSITION = 2,
        G_BUFFER_EMISSION = 3,
        G_BUFFER_AO_ROUGH_METAL = 4,
        G_BUFFER_SIZE = 5
    };

    struct create_info_t
    {
        VkExtent3D size = {};
        uint32_t num_frames_in_flight = 0;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;

        // base settings for a SceneRenderer
        SceneRenderer::settings_t settings = {};

        // convolved diffuse irradiance cube
        vierkant::ImagePtr conv_lambert;

        // convolved specular irradiance cube mipmaps
        vierkant::ImagePtr conv_ggx;
    };

    static PBRDeferredPtr create(const vierkant::DevicePtr &device, const create_info_t &create_info);

    PBRDeferred(const PBRDeferred &) = delete;

    PBRDeferred(PBRDeferred &&) = delete;

    PBRDeferred &operator=(PBRDeferred other) = delete;

    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scene       the scene to render.
     * @param   cam         the camera to use.
     * @param   tags        if not empty, only objects with at least one of the provided tags are rendered.
     * @return  the number of objects drawn
     */
    uint32_t render_scene(vierkant::Renderer &renderer,
                          const vierkant::SceneConstPtr &scene,
                          const CameraPtr &cam,
                          const std::set<std::string> &tags) override;

    /**
     * @brief   Set an environment-cubemap.
     *
     * @param   cubemap an environment-cubemap.
     */
    void set_environment(const vierkant::ImagePtr &cubemap) override;

    void set_environment(const vierkant::ImagePtr &lambert,
                         const vierkant::ImagePtr &ggx);

    vierkant::ImagePtr environment_lambert() const{ return m_conv_lambert; };

    vierkant::ImagePtr environment_ggx() const{ return m_conv_ggx; };

    /**
     * @return a const ref to the g-buffer used for last rendering.
     */
    const vierkant::Framebuffer &g_buffer() const;

    /**
     * @return a const ref to the lighting-buffer used for last rendering.
     */
    const vierkant::Framebuffer &lighting_buffer() const;

private:

    enum ShaderPropertyFlagBits
    {
        PROP_DEFAULT = 0x00,
        PROP_ALBEDO = 0x01,
        PROP_SKIN = 0x02,
        PROP_NORMAL = 0x04,
        PROP_SPEC = 0x08,
        PROP_AO_METAL_ROUGH = 0x10,
        PROP_EMMISION = 0x20
    };

    struct frame_assets_t
    {
        vierkant::Framebuffer g_buffer;
        vierkant::Framebuffer lighting_buffer;
        vierkant::BufferPtr lighting_ubo;
        vierkant::BufferPtr composition_ubo;

        //! ping-pong post-fx framebuffers
        struct ping_pong_t
        {
            vierkant::Framebuffer framebuffer;
            vierkant::Renderer renderer;
        };
        std::array<ping_pong_t, 2> post_fx_ping_pongs;

        BloomUPtr bloom;
    };

    struct environment_lighting_ubo_t
    {
        glm::mat4 camera_transform = glm::mat4(1);
        int num_mip_levels = 0;
        float env_light_strength = 1.f;
    };

    struct composition_ubo_t
    {
        float gamma = 2.2f;
        float exposure = 1.f;
        int padding[2]{};
    };

    explicit PBRDeferred(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void create_shader_stages(const DevicePtr &device);

    vierkant::Framebuffer &geometry_pass(vierkant::cull_result_t &cull_result);

    vierkant::Framebuffer &lighting_pass(const vierkant::cull_result_t &cull_result);

    void post_fx_pass(vierkant::Renderer &renderer,
                      const CameraPtr &cam,
                      const vierkant::ImagePtr &color,
                      const vierkant::ImagePtr &depth);

    static vierkant::ImagePtr create_BRDF_lut(const vierkant::DevicePtr &device);

    vierkant::DevicePtr m_device;

    vierkant::PipelineCachePtr m_pipeline_cache;

    std::unordered_map<uint32_t, vierkant::shader_stage_map_t> m_g_shader_stages;

    std::vector<frame_assets_t> m_frame_assets;

    vierkant::DrawContext m_draw_context;

    vierkant::Renderer m_g_renderer, m_light_renderer, m_post_fx_renderer;

    std::vector<VkPipelineColorBlendAttachmentState> m_g_attachment_blend_states;

    // 2d brdf lookup-table
    vierkant::ImagePtr m_brdf_lut;

    // convolved diffuse irradiance cube
    vierkant::ImagePtr m_conv_lambert;

    // convolved specular irradiance cube mipmaps
    vierkant::ImagePtr m_conv_ggx;

    vierkant::Renderer::drawable_t m_drawable_lighting_env, m_drawable_fxaa, m_drawable_dof, m_drawable_bloom;
};

}// namespace vierkant


