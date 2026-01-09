//
// Created by crocdialer on 3/20/21.
//
#pragma once

#include <deque>
#include <vierkant/Bloom.hpp>
#include <vierkant/Compute.hpp>
#include <vierkant/DrawContext.hpp>
#include <vierkant/PipelineCache.hpp>
#include <vierkant/RayBuilder.hpp>
#include <vierkant/RayTracer.hpp>
#include <vierkant/SceneRenderer.hpp>
#include <vierkant/Semaphore.hpp>
#include <vierkant/culling.hpp>
#include <vierkant/mesh_compute.hpp>


namespace vierkant
{

DEFINE_CLASS_PTR(PBRPathTracer)

class PBRPathTracer : public vierkant::SceneRenderer
{
public:
    //! group settings
    struct settings_t
    {
        //! path-tracing resolution
        glm::uvec2 resolution = {1280, 720};

        //! optional maximum number of batches to trace, default: 0 -> no limit
        uint32_t max_num_batches = 0;

        //! spp - samples per pixel
        uint32_t num_samples = 1;

        //! spp - samples per pixel
        uint32_t max_trace_depth = 6;

        //! flag indicating if path-tracing should be suspended after processing 'max_num_batches'
        bool suspend_trace_when_done = true;

        //! disable colors from textures, material, positions
        bool disable_material = false;

        //! draw the skybox, if any
        bool draw_skybox = true;

        //! flag indicating if compaction shall be used for created acceleration-structures
        bool compaction = true;

        //! flag indicating if a denoising pass shall be performed
        bool denoising = false;

        //! tonemapping
        bool tonemap = true;

        //! bloom
        bool bloom = true;

        //! factor multiplied with environment-light
        float environment_factor = 1.f;

        //! gamma correction of output
        float gamma = 1.0;

        //! exposure setting for tone-mapping
        float exposure = 2.0;

        //! enable depth of field
        bool depth_of_field = false;

        //! max number stored timing-values
        uint32_t timing_history_size = 300;
    };

    struct timings_t
    {
        RayBuilder::timings_t raybuilder_timings;

        double raytrace_ms = 0.0;
        double denoise_ms = 0.0;
        double bloom_ms = 0.0;
        double tonemap_ms = 0.0;
        double total_ms = 0.0;
    };

    struct statistics_t
    {
        std::chrono::steady_clock::time_point timestamp;
        timings_t timings;
    };

    struct create_info_t
    {
        uint32_t num_frames_in_flight = 1;

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
    render_result_t render_scene(vierkant::Rasterizer &renderer, const vierkant::SceneConstPtr &scene,
                                 const CameraPtr &cam, const std::set<std::string> &tags) override;

    std::vector<uint16_t> pick(const glm::vec2 &normalized_coord, const glm::vec2 &normalized_size) override;

    /**
     * @return the accumulator's current batch-index
     */
    size_t current_batch() const;

    /**
     * @brief   reset the accumulator
     */
    void reset_accumulator();

    /**
     * @return a queue of structs containing drawcall- and timing-results for past frames
     */
    const std::deque<statistics_t> &statistics() const { return m_statistics; }

    //! access to global settings
    settings_t settings;

private:
    enum SemaphoreValue : uint64_t
    {
        INVALID = 0,
        UPDATE_TOP,
        RAYTRACING,
        DENOISER,
        BLOOM,
        TONEMAP,
        MAX_VALUE
    };

    struct frame_context_t
    {
        settings_t settings;

        //! timeline semaphore to sync raytracing and draw-operations
        vierkant::Semaphore semaphore;

        uint64_t semaphore_value = 0;

        SemaphoreValue semaphore_value_done = SemaphoreValue::INVALID;

        //! re-usable command-buffers for all stages
        vierkant::CommandBuffer cmd_pre_render, cmd_trace, cmd_denoise, cmd_post_fx, cmd_copy_object_id;

        //! context for providing bottom-lvl acceleration structures
        RayBuilder::scene_acceleration_context_ptr scene_acceleration_context;

        //! top-lvl structure
        vierkant::RayBuilder::scene_acceleration_data_t scene_ray_acceleration;

        vierkant::RayTracer::tracable_t tracable = {};

        vierkant::Compute::computable_t denoise_computable = {};

        vierkant::ImagePtr denoise_image, out_image, out_depth;

        vierkant::BufferPtr ray_gen_ubo, ray_miss_ubo, composition_ubo;

        BloomUPtr bloom;

        //! ping-pong post-fx framebuffers
        std::array<vierkant::Framebuffer, 2> post_fx_ping_pongs;
        vierkant::Rasterizer post_fx_renderer;

        // gpu timings/statistics
        vierkant::QueryPoolPtr query_pool;
        statistics_t statistics = {};
    };

    struct alignas(16) trace_params_t
    {
        //! current time since start in seconds
        float time = 0.f;

        //! sample-batch index
        uint32_t batch_index = 0;

        //! spp - samples per pixel
        uint32_t num_samples = 1;

        //! spp - samples per pixel
        uint32_t max_trace_depth = 6;

        //! override albedo colors
        uint32_t disable_material = 0;

        //! enable skybox/background rendering
        uint32_t draw_skybox = true;

        //! a provided random seed
        uint32_t random_seed = 0;
    };

    struct denoise_params_t
    {
        glm::uvec2 size;
        VkBool32 denoise;
    };

    struct alignas(16) camera_params_t
    {
        glm::mat4 projection_view{};
        glm::mat4 projection_inverse{};
        glm::mat4 view_inverse{};
        float fov = glm::quarter_pi<float>();
        float aperture = 0.f;
        float focal_distance = 1.f;
        VkBool32 ortho = false;
    };

    struct media_t
    {
        glm::vec3 sigma_s = glm::vec3(0.f);
        float ior = 1.f;
        glm::vec3 sigma_a = glm::vec3(0.f);
        float phase_g = 0.f;
    };

    struct alignas(16) composition_ubo_t
    {
        float gamma = 2.2f;
        float exposure = 1.f;

        float time_delta = 1.f / 60.f;
        float shutter_time = 1.f / 60.f;
        float motionblur_gain = 1.f;
    };

    PBRPathTracer(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void pre_render(frame_context_t &frame_context);

    void update_acceleration_structures(frame_context_t &frame_context, const SceneConstPtr &scene,
                                        const std::set<std::string> &tags);

    void update_trace_descriptors(frame_context_t &frame_context, const CameraPtr &cam);

    void path_trace_pass(frame_context_t &frame_context, const vierkant::SceneConstPtr &scene, const CameraPtr &cam);

    void denoise_pass(frame_context_t &frame_context);

    void post_fx_pass(frame_context_t &frame_context);

    void resize_storage(frame_context_t &frame_context, const glm::uvec2 &resolution);

    //! device
    vierkant::DevicePtr m_device;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::CommandPoolPtr m_command_pool;
    vierkant::DescriptorPoolPtr m_descriptor_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    //! build acceleration structures
    vierkant::RayBuilder m_ray_builder;

    size_t m_batch_index = 0;

    //! path-tracing storage buffers and images
    struct
    {
        vierkant::BufferPtr pixel_buffer;
        vierkant::BufferPtr depth;
        vierkant::ImagePtr object_ids;
    } m_storage;

    //! owns raytracing pipelines and shader-bindingtables
    vierkant::RayTracer m_ray_tracer;

    //! owns compute pipelines
    vierkant::Compute m_compute;

    //! information for a raytracing pipeline
    raytracing_shader_map_t m_shader_stages = {}, m_shader_stages_env = {};

    std::vector<frame_context_t> m_frame_contexts;

    vierkant::DrawContext m_draw_context;

    vierkant::ImagePtr m_environment, m_empty_img;

    vierkant::drawable_t m_drawable_tonemap;

    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();

    std::default_random_engine m_random_engine;

    std::deque<statistics_t> m_statistics;
};

}// namespace vierkant
