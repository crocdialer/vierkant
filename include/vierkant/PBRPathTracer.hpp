//
// Created by crocdialer on 3/20/21.
//
#pragma once

#include "vierkant/SceneRenderer.hpp"
#include <vierkant/RayBuilder.hpp>
#include <vierkant/RayTracer.hpp>
#include <vierkant/Compute.hpp>
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

    //! group settings. not all settings are applicable in every implementation though, somewhat wip ...
    struct settings_t
    {
        //! optional maximum number of batches to trace, default: 0 -> no limit
        uint32_t max_num_batches = 0;

        //! disable colors from textures, material, vertices
        bool disable_material = false;

        //! draw the skybox, if any
        bool draw_skybox = true;

        //! flag indicating if compaction shall be used for created acceleration-structures
        bool compaction = true;

        //! flag indicating if a denoising pass shall be performed
        bool denoising = true;

        //! bloom settings
        bool use_bloom = true;

        //! gamma correction of output
        float gamma = 1.0;

        //! exposure setting for tone-mapping
        float exposure = 2.0;

        //! desired depth-of-field settings, disabled by default
        postfx::dof_settings_t dof = {};
    };

    struct create_info_t
    {
        VkExtent3D size = {};
        uint32_t num_frames_in_flight = 0;

        vierkant::PipelineCachePtr pipeline_cache = nullptr;

        VkQueue queue = VK_NULL_HANDLE;

        //! optional seed for deterministic pseudo-random-numbers
        uint32_t seed = 0;

        // settings
        settings_t settings = {};
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
     * @return  a render_result_t object.
     */
    render_result_t render_scene(vierkant::Renderer &renderer,
                                 const vierkant::SceneConstPtr &scene,
                                 const CameraPtr &cam,
                                 const std::set<std::string> &tags) override;

    /**
     * @brief   Set an environment-cubemap.
     *
     * @param   cubemap an environment-cubemap.
     */
    void set_environment(const vierkant::ImagePtr &cubemap) override;

    /**
     * @return the accumulator's current batch-index
     */
    size_t current_batch() const;

    /**
     * @brief   reset the accumulator
     */
    void reset_accumulator();

    //! access to global settings
    settings_t settings;

private:

    struct frame_assets_t
    {
        //! timeline semaphore to sync raytracing and draw-operations
        vierkant::Semaphore semaphore;

        //! records raytracing commands
        vierkant::CommandBuffer cmd_trace, cmd_denoise;

        //! pending builds for this frame
        std::unordered_map<MeshConstPtr, RayBuilder::build_result_t> build_results;

        //! maps Mesh -> bottom-lvl structures
        vierkant::RayBuilder::acceleration_asset_map_t bottom_lvl_assets;

        //! top-lvl structure
        vierkant::RayBuilder::acceleration_asset_t acceleration_asset;

        vierkant::RayTracer::tracable_t tracable = {};

        vierkant::Compute::computable_t denoise_computable = {};

        //! path-tracing storage images
        struct
        {
            vierkant::ImagePtr radiance;
            vierkant::ImagePtr normals;
            vierkant::ImagePtr positions;
            vierkant::ImagePtr accumulated_radiance;
        } storage;

        vierkant::ImagePtr denoise_image;

        vierkant::BufferPtr composition_ubo;

        vierkant::Renderer::drawable_t out_drawable;

        BloomUPtr bloom;
    };

    enum SemaphoreValue : uint64_t
    {
        ACCELERATION_UPDATE = 1,
        RAYTRACING = 2,
        DENOISER = 3,
        COMPOSITION = 4,
        RENDER_DONE = 5
    };

    struct push_constants_t
    {
        //! current time since start in seconds
        float time = 0.f;

        //! sample-batch index
        uint32_t batch_index = 0;

        //! override albedo colors
        uint32_t disable_material = 0;

        //! a provided random seed
        uint32_t random_seed = 0;
    };

    struct composition_ubo_t
    {
        float gamma = 2.2f;
        float exposure = 1.f;
        int padding[2]{};
    };

    PBRPathTracer(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void update_acceleration_structures(frame_assets_t &frame_asset,
                                        const SceneConstPtr &scene,
                                        const std::set<std::string> &tags);

    void update_trace_descriptors(frame_assets_t &frame_asset, const CameraPtr &cam);

    void path_trace_pass(frame_assets_t &frame_asset, const CameraPtr &cam);

    void denoise_pass(frame_assets_t &frame_asset);

    void post_fx_pass(frame_assets_t &frame_asset);

    //! device
    vierkant::DevicePtr m_device;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::CommandPoolPtr m_command_pool;

    //! build acceleration structures
    vierkant::RayBuilder m_ray_builder;

    bool m_compaction;

    bool m_denoising;

    vierkant::RayBuilder::acceleration_asset_map_t m_acceleration_assets;

    size_t m_batch_index = 0;

    //! owns raytracing pipelines and shader-bindingtables
    vierkant::RayTracer m_ray_tracer;

    //! owns compute pipelines
    vierkant::Compute m_compute;

    //! information for a raytracing pipeline
    raytracing_shader_map_t m_shader_stages = {}, m_shader_stages_env = {};

    std::vector<frame_assets_t> m_frame_assets;

    vierkant::DrawContext m_draw_context;

    vierkant::ImagePtr m_environment;

    vierkant::Renderer::drawable_t m_drawable_bloom, m_drawable_raw;

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();

    std::default_random_engine m_random_engine;
};

}// namespace vierkant


