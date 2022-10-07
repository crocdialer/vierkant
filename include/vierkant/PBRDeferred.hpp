//
// Created by crocdialer on 6/19/20.
//
#pragma once

#include <deque>

#include <vierkant/Compute.hpp>
#include <vierkant/GBuffer.hpp>
#include "vierkant/culling.hpp"
#include "vierkant/PipelineCache.hpp"
#include "vierkant/DrawContext.hpp"
#include "vierkant/SceneRenderer.hpp"
#include "vierkant/Bloom.hpp"
#include "vierkant/DepthOfField.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(PBRDeferred);

class PBRDeferred : public vierkant::SceneRenderer
{
public:

    //! group settings
    struct settings_t
    {
        //! internal resolution
        glm::uvec2 resolution = {1920, 1080};

        //! disable colors from textures, material, positions
        bool disable_material = false;

        //! optional flag to visualize object/meshlet indices
        bool debug_draw_ids = false;

        //! frustum-culling
        bool frustum_culling = true;

        //! occlusion-culling
        bool occlusion_culling = true;

        //! enable dynamic level-of-detail (lod) selection
        bool enable_lod = true;

        //! use tesselation
        bool tesselation = false;

        //! use wireframe rendering
        bool wireframe = false;

        //! draw the skybox, if any
        bool draw_skybox = true;

        //! apply anti-aliasing using FXAA
        bool use_fxaa = false;

        //! apply anti-aliasing using TAA
        bool use_taa = true;

        //! factor multiplied with environment-light
        float environment_factor = 1.f;

        //! use tonemapping
        bool tonemap = true;

        //! use bloom
        bool bloom = true;

        //! use motionblur
        bool motionblur = true;

        //! motionblur gain
        float motionblur_gain = 1.f;

        //! gamma correction of output
        float gamma = 1.0;

        //! exposure setting for tone-mapping
        float exposure = 2.0;

        //! indirect drawing (required for gpu-driven 'object' frustum/occlusion culling)
        bool indirect_draw = true;

        //! meshlet-based drawing (required for gpu-driven 'cluster' frustum/occlusion culling)
        bool use_meshlet_pipeline = true;

        //! max number stored timing-values
        uint32_t timing_history_size = 500;

        //! desired depth-of-field settings, disabled by default
        vierkant::dof_settings_t dof = {};
    };

    struct timings_t
    {
        double g_buffer_pre_ms = 0.0;
        double depth_pyramid_ms = 0.0;
        double culling_ms = 0.0;
        double g_buffer_post_ms = 0.0;
        double lighting_ms = 0.0;
        double taa_ms = 0.0;
        double tonemap_bloom_ms = 0.0;
        double total_ms = 0.0;
    };

    struct draw_cull_result_t
    {
        uint32_t draw_count = 0;
        uint32_t num_frustum_culled = 0;
        uint32_t num_occlusion_culled = 0;
        uint32_t num_distance_culled = 0;
        uint32_t num_triangles = 0;
    };

    struct statistics_t
    {
        std::chrono::steady_clock::time_point timestamp;
        timings_t timings;
        draw_cull_result_t draw_cull_result;
    };

    struct create_info_t
    {
        uint32_t num_frames_in_flight = 0;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        VkQueue queue = VK_NULL_HANDLE;

        // base settings for a SceneRenderer
        settings_t settings = {};

        // convolved base_color irradiance cube
        vierkant::ImagePtr conv_lambert;

        // convolved specular irradiance cube mipmaps
        vierkant::ImagePtr conv_ggx;

        vierkant::ImagePtr brdf_lut;

        std::string logger_name;
    };

    static PBRDeferredPtr create(const vierkant::DevicePtr &device, const create_info_t &create_info);

    PBRDeferred(const PBRDeferred &) = delete;

    PBRDeferred(PBRDeferred &&) = delete;

    ~PBRDeferred() override;

    PBRDeferred &operator=(PBRDeferred other) = delete;

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

    void set_environment(const vierkant::ImagePtr &lambert, const vierkant::ImagePtr &ggx);

    vierkant::ImagePtr environment_lambert() const{ return m_conv_lambert; };

    vierkant::ImagePtr environment_ggx() const{ return m_conv_ggx; };

    vierkant::ImagePtr bsdf_lut() const{ return m_brdf_lut; }

    /**
     * @return a const ref to the g-buffer used for last rendering.
     */
    const vierkant::Framebuffer &g_buffer() const;

    /**
     * @return a const ref to the lighting-buffer used for last rendering.
     */
    const vierkant::Framebuffer &lighting_buffer() const;

    /**
     * @return a queue of structs containing drawcall- and timing-results for past frames
     */
    const std::deque<statistics_t>& statistics() const { return m_statistics; }

    //! settings struct
    settings_t settings;

private:

    enum SemaphoreValue : uint64_t
    {
        G_BUFFER_LAST_VISIBLE = 1,
        DEPTH_PYRAMID,
        CULLING,
        G_BUFFER_ALL,
        LIGHTING,
        TAA,
        TONEMAP,
        DEFOCUS_BLUR,
        MAX_VALUE
    };

    struct alignas(16) camera_params_t
    {
        glm::mat4 view = glm::mat4(1);
        glm::mat4 projection = glm::mat4(1);

        glm::vec2 sample_offset;
        float near;
        float far;

        // left/right/top/bottom frustum planes
        glm::vec4 frustum;
    };

    struct frame_asset_t
    {
        std::chrono::steady_clock::time_point timestamp;

        //! contains the culled scene-drawables
        vierkant::cull_result_t cull_result;
        settings_t settings;
        std::unordered_map<vierkant::MaterialConstPtr, size_t> material_hashes;
        size_t scene_hash = 0;
        bool recycle_commands = false;
        SemaphoreValue semaphore_value_done = SemaphoreValue::G_BUFFER_ALL;
        Renderer::indirect_draw_bundle_t indirect_draw_params_pre = {}, indirect_draw_params_post = {};
        camera_params_t camera_params;

        vierkant::Semaphore timeline;
        vierkant::Framebuffer g_buffer_pre, g_buffer_post;

        vierkant::ImagePtr depth_map;

        vierkant::ImagePtr depth_pyramid;
        std::vector<vierkant::Compute> depth_pyramid_computes;
        vierkant::CommandBuffer depth_pyramid_cmd_buffer, clear_cmd_buffer, cull_cmd_buffer;
        vierkant::Compute cull_compute;
        vierkant::BufferPtr cull_ubo, cull_result_buffer, cull_result_buffer_host;

        vierkant::Framebuffer lighting_buffer, sky_buffer, taa_buffer;
        vierkant::BufferPtr bone_buffer;
        vierkant::BufferPtr morph_param_buffer;
        vierkant::BufferPtr g_buffer_camera_ubo;
        vierkant::BufferPtr lighting_param_ubo;
        vierkant::BufferPtr lights_ubo;
        vierkant::BufferPtr composition_ubo;

        // gpu timings/statistics
        vierkant::QueryPoolPtr query_pool;
        std::map<SemaphoreValue, double_millisecond_t> timings_map;
        statistics_t stats;

        //! ping-pong post-fx framebuffers
        struct ping_pong_t
        {
            vierkant::Framebuffer framebuffer;
            vierkant::Renderer renderer;
        };
        std::array<ping_pong_t, 2> post_fx_ping_pongs;

        BloomUPtr bloom;
    };

    struct alignas(16) environment_lighting_ubo_t
    {
        glm::mat4 camera_transform = glm::mat4(1);
        glm::mat4 inverse_projection = glm::mat4(1);
        uint32_t num_mip_levels = 0;
        float environment_factor = 1.f;
        uint32_t num_lights = 0;
    };

    struct alignas(16) composition_ubo_t
    {
        float gamma = 2.2f;
        float exposure = 1.f;

        float time_delta = 1.f / 60.f;
        float shutter_time = 1.f / 60.f;
        float motionblur_gain = 1.f;
    };

    struct alignas(16) draw_cull_data_t
    {
        glm::mat4 view = glm::mat4(1);

        float P00, P11, znear, zfar; // symmetric projection parameters
        glm::vec4 frustum; // data for left/right/top/bottom frustum planes

        float lod_base;
        float lod_step;

        // depth pyramid size in texels
        glm::vec2 pyramid_size = glm::vec2(0);

        uint32_t num_draws = 0;

        VkBool32 frustum_cull = false;
        VkBool32 occlusion_cull = false;
        VkBool32 distance_cull = false;
        VkBool32 backface_cull = false;
        VkBool32 lod_enabled = false;

        // buffer references
        uint64_t draw_commands_in;
        uint64_t mesh_draws_in;
        uint64_t mesh_entries_in;
        uint64_t draws_out_pre;
        uint64_t draws_out_post;
        uint64_t draw_count_pre;
        uint64_t draw_count_post;
        uint64_t draw_result;
    };

    struct node_entry_key_t
    {
        vierkant::MeshNodeConstPtr node;
        uint32_t entry_index;

        inline bool operator==(const node_entry_key_t &other) const
        {
            return node == other.node && entry_index == other.entry_index;
        }
    };

    struct node_key_hash_t
    {
        size_t operator()(node_entry_key_t const &key) const;
    };

    using matrix_cache_t = std::unordered_map<node_entry_key_t, vierkant::matrix_struct_t, node_key_hash_t>;

    explicit PBRDeferred(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void update_timing(frame_asset_t &frame_asset);

    void update_recycling(const SceneConstPtr &scene,
                          const CameraPtr &cam, frame_asset_t &frame_asset) const;

    void update_matrix_history(frame_asset_t &frame_asset);

    void resize_storage(frame_asset_t &frame_asset, const glm::uvec2 &resolution);

    vierkant::Framebuffer &geometry_pass(vierkant::cull_result_t &cull_result);

    vierkant::Framebuffer &lighting_pass(const vierkant::cull_result_t &cull_result);

    void post_fx_pass(vierkant::Renderer &renderer,
                      const CameraPtr &cam,
                      const vierkant::ImagePtr &color,
                      const vierkant::ImagePtr &depth);

    void create_depth_pyramid(frame_asset_t &frame_asset);

    void cull_draw_commands(frame_asset_t &frame_asset,
                            const vierkant::CameraPtr &cam,
                            const vierkant::ImagePtr &depth_pyramid,
                            uint32_t num_draws,
                            const vierkant::BufferPtr &draws_in,
                            const vierkant::BufferPtr &mesh_draws_in,
                            const vierkant::BufferPtr &mesh_entries_in,
                            vierkant::BufferPtr &draws_out,
                            vierkant::BufferPtr &draws_counts_out,
                            vierkant::BufferPtr &draws_out_post,
                            vierkant::BufferPtr &draws_counts_out_post);

    void resize_indirect_draw_buffers(uint32_t num_draws,
                                      Renderer::indirect_draw_bundle_t &params);

    vierkant::DevicePtr m_device;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    g_buffer_stage_map_t m_g_buffer_shader_stages;

    std::vector<glm::vec2> m_sample_offsets;

    size_t m_sample_index = 0;

    std::vector<frame_asset_t> m_frame_assets;

    vierkant::DrawContext m_draw_context;

    vierkant::Renderer m_g_renderer_pre, m_g_renderer_post;

    vierkant::Renderer m_light_renderer, m_sky_renderer, m_taa_renderer;

    // 2d brdf lookup-table
    vierkant::ImagePtr m_brdf_lut;

    // convolved base_color irradiance cube
    vierkant::ImagePtr m_conv_lambert;

    // convolved specular irradiance cube mipmaps
    vierkant::ImagePtr m_conv_ggx;

    // helper, empty image
    vierkant::ImagePtr m_empty_img;

    vierkant::drawable_t m_drawable_lighting_env, m_drawable_fxaa, m_drawable_dof, m_drawable_bloom, m_drawable_taa;

    vierkant::Compute::computable_t m_depth_pyramid_computable;
    glm::uvec3 m_depth_pyramid_local_size{0};

    vierkant::Compute::computable_t m_cull_computable;
    glm::uvec3 m_cull_compute_local_size{0};

    // cache matrices and bones from previous frame
    matrix_cache_t m_entry_matrix_cache;

    // a logger
    std::shared_ptr<spdlog::logger> m_logger;

    std::deque<statistics_t> m_statistics;
};

extern bool operator==(const PBRDeferred::settings_t &lhs, const PBRDeferred::settings_t &rhs);

inline bool operator!=(const PBRDeferred::settings_t &lhs, const PBRDeferred::settings_t &rhs){ return !(lhs == rhs); }

}// namespace vierkant


