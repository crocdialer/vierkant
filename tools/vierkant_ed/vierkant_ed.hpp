//
// Created by crocdialer on 9/1/18.
//

#pragma once

#include "scene_data.hpp"
#include <crocore/Application.hpp>
#include <cereal/types/memory.hpp>// OrbitCameraPtr / FlyCameraPtr

#include <crocore/set_lru.hpp>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <vierkant/CameraControl.hpp>
#include <vierkant/bundle.hpp>
#include <vierkant/PBRDeferred.hpp>
#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/imgui/imgui_util.h>
#include <vierkant/object_overlay.hpp>
#include <vierkant/physics_context.hpp>
#include <vierkant/physics_debug_draw.hpp>

class VierkantEd : public crocore::Application
{

public:
    //! which of the camera-controls is active
    enum class CameraControlMode : uint32_t
    {
        Orbit = 0,
        Fly
    };

    struct settings_t
    {
        spdlog::level::level_enum log_level = spdlog::level::info;
        std::string log_file;
        bool use_validation = false;
        bool use_debug_labels = false;
        crocore::set_lru<std::string> recent_files;

        vierkant::Window::create_info_t window_info = {.instance = VK_NULL_HANDLE,
                                                       .size = {1920, 1080},
                                                       .position = {},
                                                       .fullscreen = false,
                                                       .vsync = true,
                                                       .joysticks = true,
                                                       .monitor_index = 0,
                                                       .sample_count = VK_SAMPLE_COUNT_1_BIT,
                                                       .title = "vierkant_ed"};

        vierkant::PBRDeferred::settings_t pbr_settings = {};
        vierkant::PBRPathTracer::settings_t path_tracer_settings = {};

        vierkant::mesh_buffer_params_t mesh_buffer_params = {.remap_indices = false,
                                                             .optimize_vertex_cache = true,
                                                             .generate_lods = false,
                                                             .generate_meshlets = false,
                                                             .pack_vertices = true};

        bool draw_ui = true;

        float ui_scale = 1.f;

        std::string font_url;

        float ui_font_scale = 30.f;

        bool ui_draw_view_controls = false;

        bool draw_grid = true;

        bool draw_aabbs = false;

        bool draw_physics = false;

        bool draw_node_hierarchy = false;

        bool path_tracing = false;

        bool texture_compression = false;

        //! bake opacity-micromaps (OMM) for alpha-masked geometry and feed them to the path-tracer
        bool opacity_micromaps = false;

        bool cache_mesh_bundles = false;

        bool cache_zip_archive = false;

        bool enable_raytracing_pipeline_features = true;

        bool enable_ray_query_features = true;

        bool enable_mesh_shader_device_features = true;

        vierkant::OrbitCameraPtr orbit_camera = vierkant::OrbitCamera::create();
        vierkant::FlyCameraPtr fly_camera = vierkant::FlyCamera::create();
        CameraControlMode camera_control = CameraControlMode::Orbit;
        bool ortho_camera = false;

        //! route gamepad-input to the first character in the scene, independent of the camera-control
        bool character_input = false;

        //! the character-body turns with the view-yaw. pitch is never routed to a body, it would tilt it.
        //! when unset the eye takes the yaw instead, and the body keeps its orientation
        bool body_use_view_yaw = true;

        vierkant::gui::GuizmoType current_guizmo = vierkant::gui::GuizmoType::INACTIVE;
        vierkant::gui::GuizmoSpace guizmo_space = vierkant::gui::GuizmoSpace::WORLD;

        vierkant::ObjectOverlayMode object_overlay_mode = vierkant::ObjectOverlayMode::Mask;

        //! desired fps, default: 0.f (disable throttling)
        float target_fps = 60.f;

        float playback_speed = 1.f;
        bool animation_playback = true;
        bool physics_playback = true;
    };

    static constexpr char s_default_scene_path[] = "scene.json";

    explicit VierkantEd(const crocore::Application::create_info_t &create_info);

    void load_file(const std::string &path, bool clear);

    bool parse_override_settings(int argc, char *argv[]);

private:
    void setup() override;

    void update(double time_delta) override;

    void teardown() override;

    void poll_events() override;

    vierkant::window_delegate_t::draw_result_t draw(const vierkant::WindowPtr &w);

    void init_logger();

    void create_context_and_window();

    void create_graphics_pipeline();

    void create_ui();

    void create_camera_controls();

    //! the camera-controls drive the editor-camera only. while the scene is rendered through one of
    //! its own cameras they are inert, rather than silently moving a camera nobody is looking through.
    [[nodiscard]] inline bool editor_camera_active() const { return m_render_camera == m_editor_camera; }

    //! feed the player-control's input into the first object carrying a vierkant::character_t
    void update_player_input(double time_delta);

    void update_js(double time_delta);

    void create_texture_image();

    void add_to_recent_files(const std::filesystem::path &f);

    struct load_model_params_t
    {
        //! model-path
        std::filesystem::path path = {};

        //! load a model as mesh-library, containing individual sub-object per mesh-entry
        bool mesh_library = false;

        //! when loading as mesh-library, avoid duplicated objects for identical entries
        bool mesh_library_no_dups = false;

        //! normalize dimensions of loaded assets
        bool normalize_size = false;

        //! clear the scene when loading-operation succeeds
        bool clear_scene = false;
    };
    void load_model(const load_model_params_t &params);

    //! result of a cached image-load: the id derived from the source-key, plus the gpu-texture
    struct load_texture_result_t
    {
        vierkant::TextureId texture_id = vierkant::TextureId::nil();
        vierkant::ImagePtr gpu_texture;
    };

    //! load an image from a project-key, via the texture-cache. bakes and caches on a miss.
    load_texture_result_t load_texture_asset(const std::string &key);

    void load_texture(const std::string &path);

    void load_environment(const std::string &path);

    void save_settings(settings_t settings, const std::filesystem::path &path = "settings.json") const;

    static std::optional<settings_t> load_settings(const std::filesystem::path &path = "settings.json");

    void save_asset_bundle(const vierkant::model::model_assets_t &mesh_assets, const std::filesystem::path &path) const;

    std::optional<vierkant::model::model_assets_t> load_asset_bundle(const std::filesystem::path &path) const;

    void save_material_bundle(const vierkant::material_data_t &material_data, const std::filesystem::path &path) const;

    std::optional<vierkant::material_data_t> load_material_bundle(const std::filesystem::path &path) const;

    void save_texture_bundle(const vierkant::texture_variant_t &texture, const std::filesystem::path &path) const;

    std::optional<vierkant::texture_variant_t> load_texture_bundle(const std::filesystem::path &path) const;

    void save_environment_bundle(const vierkant::environment_assets_t &assets, const std::filesystem::path &path) const;

    std::optional<vierkant::environment_assets_t> load_environment_bundle(const std::filesystem::path &path) const;

    //! project-root helpers (P1). establish the root once from the top-scene (or --project-root).
    void establish_project_root(const std::filesystem::path &top_scene_path);

    //! convert an on-disk path into a portable, root-relative asset-key (forward-slashes). paths
    //! outside the project-root are kept absolute and a warning is logged. this is the single string
    //! persisted in scene-data and fed to `from_name`. only apply to freshly-ingested filesystem paths.
    std::string project_key(const std::filesystem::path &p) const;

    //! resolve an asset-key back to an openable path (relative keys join the project-root).
    std::filesystem::path resolve(const std::string &key) const;

    //! derived (texture-)bundle path for a scene, under the project-root cache.
    std::filesystem::path material_bundle_path(const std::string &scene_path) const;

    //! derived cache-path for an imported image, under the project-root cache.
    std::filesystem::path texture_bundle_path(const std::string &image_key) const;

    //! derived cache-path for an environment-map's cubemaps, under the project-root cache.
    std::filesystem::path environment_bundle_path(const std::string &image_key) const;

    //! optional zip-archive path under the project-root, depending on the cache_zip_archive setting.
    std::optional<std::filesystem::path> zip_archive_path() const;

    vierkant::model::load_mesh_result_t load_mesh(const std::filesystem::path &path);

    void save_scene(std::filesystem::path path = {});

    static std::optional<scene_data_t> load_scene_data(const std::filesystem::path &path = s_default_scene_path);

    void build_scene(const std::optional<scene_data_t> &scene_data, bool import = false,
                     vierkant::SceneId scene_id = {});

    //! clone a set of objects, assigning fresh physics body-ids and remapping their constraints.
    //! when 'instance_seed' is provided, new body-ids are derived deterministically from it (stable
    //! across reloads, e.g. for sub-scene instances); otherwise fresh random ids are used (e.g. copy/paste).
    std::vector<vierkant::Object3DPtr> clone_objects(const std::set<vierkant::Object3DPtr> &objects,
                                                     const std::optional<std::string> &instance_seed = {}) const;

    struct overlay_assets_t
    {
        vierkant::CommandBuffer command_buffer;
        vierkant::Semaphore semaphore;
        uint64_t semaphore_value = 0;
        vierkant::object_overlay_context_ptr object_overlay_context;
        vierkant::SceneRenderer::object_id_by_index_fn_t object_by_index_fn;
        vierkant::SceneRenderer::indices_by_id_fn_t indices_by_id_fn;
        vierkant::ImagePtr overlay;
    };

    vierkant::semaphore_submit_info_t generate_overlay(overlay_assets_t &overlay_asset,
                                                       const vierkant::ImagePtr &id_img);

    void toggle_ortho_camera();

    std::atomic<uint32_t> m_num_loading = 0, m_num_frames = 0;

    settings_t m_settings = {};

    // bundles basic Vulkan assets
    vierkant::Instance m_instance;

    // device
    vierkant::DevicePtr m_device;

    VkQueue m_queue_model_loading = VK_NULL_HANDLE, m_queue_image_loading = VK_NULL_HANDLE,
            m_queue_render = VK_NULL_HANDLE;

    //! format for HDR render-targets (deferred lighting + post-fx). has to stay renderable.
    // B10G11R11 saves 50% memory but now seeing more&more cases with strong banding-issues
    VkFormat m_hdr_render_format = VK_FORMAT_R16G16B16A16_SFLOAT;//VK_FORMAT_B10G11R11_UFLOAT_PACK32;

    //! format the environment-cubemaps are baked in. they are stored and sampled block-compressed,
    //! so this is the renderable format the convolutions render into, not what ends up resident.
    VkFormat m_hdr_texture_format = VK_FORMAT_R16G16B16A16_SFLOAT;

    //! edge-length of the diffuse (lambert) environment-convolution. folded into the environment
    //! cache-key, so changing it re-bakes.
    static constexpr uint32_t s_lambert_size = 128;

    VkBufferUsageFlags m_mesh_buffer_flags = 0;

    vierkant::material_t m_primitive_material;
    const vierkant::TextureId m_primitive_texture_id = vierkant::TextureId::from_name("primitive_texture");
    const vierkant::TextureId m_noise_texture_id = vierkant::TextureId::from_name("noise_texture");
    vierkant::ImagePtr m_primitive_texture, m_environment_texture, m_noise_texture;

    //! host-side texture/sampler store kept for bundle-serialization;
    //! materials + the GPU-side runtime store are owned by the AssetProvider (m_asset_provider)
    vierkant::material_data_t m_material_data;

    // window handle
    vierkant::WindowPtr m_window;

    // init a scene with physics-support on application-threadpool
    std::shared_ptr<vierkant::ObjectStore> m_object_store = vierkant::create_object_store(1 << 20);

    //! owns the material-library + GPU runtime asset store; handed to the scene below
    vierkant::AssetProviderPtr m_asset_provider = vierkant::AssetProvider::create();
    std::shared_ptr<vierkant::PhysicsScene> m_scene = vierkant::PhysicsScene::create(m_object_store, m_asset_provider);
    vierkant::PhysicsDebugRendererPtr m_physics_debug;

    //! viewport-camera, driven by the camera-controls. lives in the scene-graph on LAYER_EDITOR,
    //! which keeps it out of rendering, physics and serialization
    vierkant::Object3DPtr m_editor_camera;

    //! camera the scene is drawn through, the editor-camera or one picked from the scene
    vierkant::Object3DPtr m_render_camera;

    struct camera_control_t
    {
        vierkant::OrbitCameraPtr orbit = vierkant::OrbitCamera::create();
        vierkant::FlyCameraPtr fly = vierkant::FlyCamera::create();
        vierkant::CameraControlPtr current = orbit;
    } m_camera_control;

    //! gamepad-input for a character, does not drive the camera
    vierkant::PlayerControlPtr m_player_control = vierkant::PlayerControl::create();

    std::vector<vierkant::Joystick> m_fly_joystick_states;

    // object-selection / copy/paste
    std::set<vierkant::Object3DPtr> m_selected_objects;
    std::set<vierkant::Object3DPtr> m_copy_objects;
    std::unordered_set<uint32_t> m_selected_indices;
    std::optional<crocore::Area_<int>> m_selection_area;

    vierkant::PipelineCachePtr m_pipeline_cache;

    // selection of scene-renderers
    vierkant::PBRDeferredPtr m_pbr_renderer;

    vierkant::PBRPathTracerPtr m_path_tracer;

    vierkant::SceneRendererPtr m_scene_renderer;

    vierkant::Rasterizer m_renderer, m_renderer_overlay, m_renderer_gui;

    std::vector<overlay_assets_t> m_overlay_assets;
    vierkant::ImagePtr m_object_id_image;

    vierkant::gui::Context m_gui_context;

    // some internal UI-state
    std::unique_ptr<struct ui_state_t, std::function<void(struct ui_state_t *)>> m_ui_state;

    vierkant::DrawContext m_draw_context;

    size_t m_max_log_queue_size = 100;
    std::deque<std::pair<std::string, spdlog::level::level_enum>> m_log_queue;
    std::shared_mutex m_log_queue_mutex, m_mutex_semaphore_submit;
    std::map<std::string, std::shared_ptr<spdlog::logger>> _loggers;

    scene_data_t m_scene_data;

    //! scene-level CPU opacity-micromap cache; accumulated across loaded meshes and
    //! handed (non-owning) to the path-tracer via settings.omm_cache
    vierkant::model::mesh_omm_cache_t m_scene_omm_cache;

    //! project-root all scene-data asset-paths are stored relative to and resolved against (P1).
    //! established once per session from the top-scene dir, CWD, or an explicit --project-root.
    std::filesystem::path m_project_root = std::filesystem::current_path();
    bool m_project_root_explicit = false;

    // track of scene/model/image-paths (stored as root-relative asset-keys, see project_key/resolve)
    std::map<vierkant::MeshId, std::filesystem::path> m_model_paths;
    std::map<vierkant::SceneId, std::filesystem::path> m_scene_paths;
    std::map<vierkant::TextureId, std::filesystem::path> m_texture_paths;
    vierkant::SceneId m_scene_id;
};

#include "scene_cereal.hpp"

template<class Archive>
void serialize(Archive &ar, VierkantEd::settings_t &settings)
{
    ar(cereal::make_nvp("use_validation", settings.use_validation),
       cereal::make_nvp("use_debug_labels", settings.use_debug_labels),
       cereal::make_nvp("log_level", settings.log_level), cereal::make_nvp("log_file", settings.log_file),
       cereal::make_nvp("recent_files", settings.recent_files), cereal::make_nvp("window", settings.window_info),
       cereal::make_nvp("pbr_settings", settings.pbr_settings),
       cereal::make_nvp("path_tracer_settings", settings.path_tracer_settings),
       cereal::make_nvp("draw_ui", settings.draw_ui),
       cereal::make_nvp("ui_draw_view_controls", settings.ui_draw_view_controls),
       cereal::make_nvp("font_url", settings.font_url), cereal::make_nvp("ui_scale", settings.ui_scale),
       cereal::make_nvp("ui_font_scale", settings.ui_font_scale), cereal::make_nvp("draw_grid", settings.draw_grid),
       cereal::make_nvp("draw_aabbs", settings.draw_aabbs), cereal::make_nvp("draw_physics", settings.draw_physics),
       cereal::make_nvp("draw_node_hierarchy", settings.draw_node_hierarchy),
       cereal::make_nvp("path_tracing", settings.path_tracing),
       cereal::make_nvp("texture_compression", settings.texture_compression),
       cereal::make_optional_nvp("opacity_micromaps", settings.opacity_micromaps),
       cereal::make_nvp("mesh_buffer_params", settings.mesh_buffer_params),
       cereal::make_nvp("cache_mesh_bundles", settings.cache_mesh_bundles),
       cereal::make_nvp("cache_zip_archive", settings.cache_zip_archive),
       cereal::make_nvp("enable_raytracing_pipeline_features", settings.enable_raytracing_pipeline_features),
       cereal::make_nvp("enable_ray_query_features", settings.enable_ray_query_features),
       cereal::make_nvp("enable_mesh_shader_device_features", settings.enable_mesh_shader_device_features),
       cereal::make_nvp("orbit_camera", settings.orbit_camera), cereal::make_nvp("fly_camera", settings.fly_camera),
       cereal::make_optional_nvp("camera_control", settings.camera_control,
                                 VierkantEd::CameraControlMode::Orbit),
       cereal::make_nvp("ortho_camera", settings.ortho_camera),
       cereal::make_optional_nvp("character_input", settings.character_input, false),
       cereal::make_optional_nvp("body_use_view_yaw", settings.body_use_view_yaw, true),
       cereal::make_nvp("current_guizmo", settings.current_guizmo),
       cereal::make_optional_nvp("guizmo_space", settings.guizmo_space, vierkant::gui::GuizmoSpace::WORLD),
       cereal::make_nvp("object_overlay_mode", settings.object_overlay_mode),
       cereal::make_nvp("target_fps", settings.target_fps),
       cereal::make_optional_nvp("playback_speed", settings.playback_speed, 1.f),
       cereal::make_optional_nvp("animation_playback", settings.animation_playback, true),
       cereal::make_optional_nvp("physics_playback", settings.physics_playback, true));
}