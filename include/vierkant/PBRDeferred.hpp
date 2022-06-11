//
// Created by crocdialer on 6/19/20.
//
#pragma once

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

        //! frustum-culling
        bool frustum_culling = true;

        //! occlusion-culling
        bool occlusion_culling = true;

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

        //! desired depth-of-field settings, disabled by default
        vierkant::dof_settings_t dof = {};
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
        POST_FX,
        TONEMAP,
        DONE = G_BUFFER_LAST_VISIBLE// TODO: wip all semaphore-stages
    };

    struct alignas(16) camera_params_t
    {
        glm::mat4 view = glm::mat4(1);
        glm::mat4 projection = glm::mat4(1);

        glm::vec2 sample_offset;
        float near;
        float far;
    };

    struct frame_assets_t
    {
        //! contains the culled scene-drawables
        vierkant::cull_result_t cull_result;
        settings_t settings;
        std::unordered_map<vierkant::MaterialConstPtr, size_t> material_hashes;
        size_t scene_hash = 0;
        bool recycle_commands = false;
        Renderer::indirect_draw_params_t indirect_draw_params_pre = {}, indirect_draw_params_post = {};
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
        vierkant::BufferPtr g_buffer_camera_ubo;
        vierkant::BufferPtr lighting_param_ubo;
        vierkant::BufferPtr lights_ubo;
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

        // depth pyramid size in texels
        glm::vec2 pyramid_size = glm::vec2(0);

        uint32_t num_draws = 0;

        VkBool32 culling_enabled = false;
        VkBool32 lod_enabled = false;
        VkBool32 occlusion_enabled = false;
        VkBool32 distance_cull = false;
    };

    struct alignas(16) draw_cull_result_t
    {
        uint32_t draw_count = 0;
        uint32_t num_frustum_culled = 0;
        uint32_t num_occlusion_culled = 0;
        uint32_t num_distance_culled = 0;
        uint32_t num_triangles = 0;
    };

    struct matrix_key_t
    {
        vierkant::MeshConstPtr mesh;
        uint32_t entry_index;

        inline bool operator==(const matrix_key_t &other) const
        {
            return mesh == other.mesh && entry_index == other.entry_index;
        }
    };

    struct matrix_key_hash_t
    {
        size_t operator()(matrix_key_t const &key) const;
    };

    using matrix_cache_t = std::unordered_map<matrix_key_t, Renderer::matrix_struct_t, matrix_key_hash_t>;

    explicit PBRDeferred(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void update_recycling(const SceneConstPtr &scene,
                          const CameraPtr &cam,
                          frame_assets_t &frame_asset) const;

    void update_matrix_history(frame_assets_t &frame_asset);

    void resize_storage(frame_assets_t &frame_asset, const glm::uvec2 &resolution);

    vierkant::Framebuffer &geometry_pass(vierkant::cull_result_t &cull_result);

    vierkant::Framebuffer &lighting_pass(const vierkant::cull_result_t &cull_result);

    void post_fx_pass(vierkant::Renderer &renderer,
                      const CameraPtr &cam,
                      const vierkant::ImagePtr &color,
                      const vierkant::ImagePtr &depth);

    void create_depth_pyramid(frame_assets_t &frame_asset);

    void cull_draw_commands(frame_assets_t &frame_asset,
                            const vierkant::CameraPtr &cam,
                            const vierkant::ImagePtr &depth_pyramid,
                            const vierkant::BufferPtr &draws_in,
                            uint32_t num_draws,
                            vierkant::BufferPtr &draws_out,
                            vierkant::BufferPtr &draws_counts_out,
                            vierkant::BufferPtr &draws_out_post,
                            vierkant::BufferPtr &draws_counts_out_post);

    void resize_indirect_draw_buffers(uint32_t num_draws,
                                      Renderer::indirect_draw_params_t &params,
                                      VkCommandBuffer clear_cmd_handle = VK_NULL_HANDLE);

    vierkant::DevicePtr m_device;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    g_buffer_stage_map_t m_g_buffer_shader_stages;

    std::vector<glm::vec2> m_sample_offsets;

    size_t m_sample_index = 0;

    std::vector<frame_assets_t> m_frame_assets;

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

    vierkant::Renderer::drawable_t m_drawable_lighting_env, m_drawable_fxaa, m_drawable_dof, m_drawable_bloom,
            m_drawable_taa;

    vierkant::Compute::computable_t m_depth_pyramid_computable;
    glm::uvec3 m_depth_pyramid_local_size{0};

    vierkant::Compute::computable_t m_cull_computable;
    glm::uvec3 m_cull_compute_local_size{0};

    // cache matrices and bones from previous frame
    matrix_cache_t m_entry_matrix_cache;

    // keep track of frame-times
    std::chrono::steady_clock::time_point m_timestamp_current = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point m_timestamp_last = m_timestamp_current;

    // a logger
    std::shared_ptr<spdlog::logger> m_logger;
};

extern bool operator==(const PBRDeferred::settings_t &lhs, const PBRDeferred::settings_t &rhs);

inline bool operator!=(const PBRDeferred::settings_t &lhs, const PBRDeferred::settings_t &rhs){ return !(lhs == rhs); }

}// namespace vierkant


