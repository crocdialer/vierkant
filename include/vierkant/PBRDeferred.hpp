//
// Created by crocdialer on 6/19/20.
//
#pragma once

#include "vierkant/culling.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/DrawContext.hpp"
#include "vierkant/SceneRenderer.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(PBRDeferred);

class PBRDeferred : public vierkant::SceneRenderer
{
public:

    struct create_info_t
    {
        VkExtent3D size = {};
        uint32_t num_frames_in_flight = 0;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
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

private:

    enum G_BUFFER
    {
        G_BUFFER_ALBEDO = 0,
        G_BUFFER_NORMAL = 1,
        G_BUFFER_POSITION = 2,
        G_BUFFER_EMISSION = 3,
        G_BUFFER_AO_ROUGH_METAL = 4,
        G_BUFFER_SIZE = 5
    };

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
    };

    explicit PBRDeferred(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void create_shader_stages(const DevicePtr &device);

    static vierkant::ImagePtr create_BRDF_lut(const vierkant::DevicePtr &device);

    vierkant::Framebuffer& geometry_pass(vierkant::cull_result_t &cull_result);

    void lighting_pass(const vierkant::cull_result_t &cull_result);

    vierkant::PipelineCachePtr m_pipeline_cache;

    std::unordered_map<uint32_t, vierkant::shader_stage_map_t> m_shader_stages;

    std::vector<frame_assets_t> m_frame_assets;

    vierkant::DrawContext m_draw_context;

    vierkant::Renderer m_g_renderer;

    // 2d brdf lookup-table
    vierkant::ImagePtr m_brdf_lut;

    // convolved diffuse iradiance cube
    vierkant::ImagePtr m_conv_diffuse;

    // convolved specular iradiance cube mipmaps
    vierkant::ImagePtr m_conv_spec;
};

}// namespace vierkant


