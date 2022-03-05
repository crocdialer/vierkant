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

    //! group settings. not all settings are applicable in every implementation though, somewhat wip ...
    struct settings_t
    {
        //! internal resolution
        glm::uvec2 resolution = {1920, 1080};

        //! disable colors from textures, material, positions
        bool disable_material = false;

        //! draw the skybox, if any
        bool draw_skybox = true;

        //! apply anti-aliasing using FXAA
        bool use_fxaa = false;

        //! apply anti-aliasing using TAA
        bool use_taa = true;

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
        VkExtent3D size = {};
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

    void set_environment(const vierkant::ImagePtr &lambert,
                         const vierkant::ImagePtr &ggx);

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
        CULLING = 1,
        G_BUFFER,
        LIGHTING,
        POST_FX,
        TONEMAP
    };

    struct frame_assets_t
    {
        vierkant::Semaphore timeline;
        glm::vec2 jitter_offset;
        vierkant::Framebuffer g_buffer;

        vierkant::ImagePtr depth_map;

        vierkant::ImagePtr depth_pyramid;
        std::vector<vierkant::Compute> depth_pyramid_computes;
        vierkant::CommandBuffer depth_pyramid_cmd_buffer;
        vierkant::Compute cull_compute;

        vierkant::Framebuffer lighting_buffer, sky_buffer, taa_buffer;
        vierkant::BufferPtr g_buffer_ubo;
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

    struct alignas(16) taa_ubo_t
    {
        float near;
        float far;
        glm::vec2 sample_offset;
    };

    struct alignas(16) environment_lighting_ubo_t
    {
        glm::mat4 camera_transform = glm::mat4(1);
        glm::mat4 inverse_projection = glm::mat4(1);
        int num_mip_levels = 0;
        float env_light_strength = 1.f;
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
        float frustum[4]; // data for left/right/top/bottom frustum planes

        // depth pyramid size in texels
        glm::vec2 pyramid_size = glm::vec2(0);

        uint32_t draw_count = 0;

        VkBool32 culling_enabled = false;
        VkBool32 lod_enabled = false;
        VkBool32 occlusion_enabled = false;
        VkBool32 distance_cull = false;

        VkBool32 AABB_check = false;
        glm::vec3 aabb_min = glm::vec3(0);
        glm::vec3 aabb_max = glm::vec3(0);
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

    using bone_buffer_cache_t = std::unordered_map<vierkant::nodes::NodeConstPtr, vierkant::BufferPtr>;

    explicit PBRDeferred(const vierkant::DevicePtr &device, const create_info_t &create_info);

    void update_matrix_history(vierkant::cull_result_t &cull_result);

    void resize_storage(frame_assets_t &frame_asset, const glm::uvec2 &resolution);

    vierkant::Framebuffer &geometry_pass(vierkant::cull_result_t &cull_result);

    vierkant::Framebuffer &lighting_pass(const vierkant::cull_result_t &cull_result);

    void post_fx_pass(vierkant::Renderer &renderer,
                      const CameraPtr &cam,
                      const vierkant::ImagePtr &color,
                      const vierkant::ImagePtr &depth);

    static vierkant::ImagePtr create_BRDF_lut(const vierkant::DevicePtr &device);

    void update_bone_uniform_buffer(const vierkant::MeshConstPtr &mesh, vierkant::BufferPtr &out_buffer);

    void create_depth_pyramid(frame_assets_t &frame_asset);

    void digest_draw_command_buffer(frame_assets_t &frame_asset,
                                    VkCommandBuffer cmd_buffer,
                                    uint32_t num_draws,
                                    const vierkant::BufferPtr &draws_in,
                                    vierkant::BufferPtr &draws_out);

    vierkant::DevicePtr m_device;

    VkQueue m_queue = VK_NULL_HANDLE;

    vierkant::CommandPoolPtr m_command_pool;

    vierkant::PipelineCachePtr m_pipeline_cache;

    g_buffer_stage_map_t m_g_buffer_shader_stages;

    std::vector<glm::vec2> m_sample_offsets;

    size_t m_sample_index = 0;

    std::vector<frame_assets_t> m_frame_assets;

    vierkant::DrawContext m_draw_context;

    vierkant::Renderer m_g_renderer, m_light_renderer, m_sky_renderer, m_taa_renderer;

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
    bone_buffer_cache_t m_bone_buffer_cache;

    // keep track of frame-times
    std::chrono::steady_clock::time_point m_timestamp_current = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point m_timestamp_last = m_timestamp_current;
};

}// namespace vierkant


