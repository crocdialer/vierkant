//
// Created by crocdialer on 3/20/21.
//
#pragma once

#include "vierkant/SceneRenderer.hpp"
#include <vierkant/RayBuilder.hpp>
#include <vierkant/RayTracer.hpp>
#include <vierkant/Semaphore.hpp>
#include "vierkant/culling.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/Bloom.hpp"
#include "DrawContext.hpp"


namespace vierkant
{

DEFINE_CLASS_PTR(PBRPathTracer);

class PBRPathTracer : public vierkant::SceneRenderer
{
public:

    struct create_info_t
    {
        VkExtent3D size = {};
        uint32_t num_frames_in_flight = 0;

        vierkant::PipelineCachePtr pipeline_cache = nullptr;

        VkQueue queue = VK_NULL_HANDLE;

        // base settings for a SceneRenderer
        SceneRenderer::settings_t settings = {};
    };

    static PBRPathTracerPtr create(const vierkant::DevicePtr &device, const create_info_t &create_info);

    PBRPathTracer(const PBRPathTracer &) = delete;

    PBRPathTracer(PBRPathTracer &&) = delete;

    PBRPathTracer &operator=(PBRPathTracer other) = delete;

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

private:

    struct frame_assets_t
    {
        //! timeline semaphore to sync raytracing and draw-operations
        vierkant::Semaphore semaphore;

        //! records raytracing commands
        vierkant::CommandBuffer command_buffer;

        //! an accelaration structure and it's resources
        vierkant::RayBuilder::acceleration_asset_t acceleration_asset;

        vierkant::RayTracer::tracable_t tracable = {};

        vierkant::ImagePtr storage_image;

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

    enum SemaphoreValue
    {
        RAYTRACING_FINISHED = 1,
        RENDER_FINISHED = 2
    };

    struct composition_ubo_t
    {
        float gamma = 2.2f;
        float exposure = 1.f;
        int padding[2]{};
    };

    PBRPathTracer(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void update_trace_descriptors(frame_assets_t & frame_asset, const CameraPtr &cam);

    void path_trace_pass(frame_assets_t &frame_asset, const CameraPtr &cam);

    vierkant::ImagePtr post_fx_pass(frame_assets_t &frame_asset);

    //! device
    vierkant::DevicePtr m_device;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::CommandPoolPtr m_command_pool;

    //! build acceleration structures
    vierkant::RayBuilder m_ray_builder;

    //! owns raytracing pipeline and shader-bindingtable
    vierkant::RayTracer m_ray_tracer;

    //! information for a raytracing pipeline
    raytracing_shader_map_t m_shader_stages = {}, m_shader_stages_env = {};

    std::vector<frame_assets_t> m_frame_assets;

    vierkant::DrawContext m_draw_context;

    vierkant::ImagePtr m_environment;

    vierkant::Renderer::drawable_t m_composition_drawable;
};

}// namespace vierkant

