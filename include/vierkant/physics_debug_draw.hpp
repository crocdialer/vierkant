#pragma once

#include <vierkant/DrawContext.hpp>
#include <vierkant/SceneRenderer.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(PhysicsDebugRenderer)

class PhysicsDebugRenderer : public vierkant::SceneRenderer
{
public:
    struct settings_t
    {
        //! internal resolution
        glm::uvec2 resolution = {1920, 1080};
        bool draw_aabbs = true;
        bool draw_meshes = true;
        bool draw_lines = true;
        bool use_mesh_colors = true;
        glm::vec4 overlay_color = glm::vec4(1.f, 1.f, 1.f, .6f);

        constexpr bool operator==(const settings_t &lhs) const = default;
        constexpr bool operator!=(const settings_t &lhs) const = default;
    };

    struct create_info_t
    {
        vierkant::DevicePtr device;
        settings_t settings = {};
        uint32_t num_frames_in_flight = 1;
        VkQueue queue = VK_NULL_HANDLE;
        VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
    };

    settings_t settings = {};

    static PhysicsDebugRendererPtr create(const create_info_t &create_info);

    PhysicsDebugRenderer(const PhysicsDebugRenderer &) = delete;

    PhysicsDebugRenderer(PhysicsDebugRenderer &&) = delete;

    PhysicsDebugRenderer &operator=(PhysicsDebugRenderer other) = delete;

    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scene       the scene to render.
     * @param   cam         the camera to use.
     * @param   tags        if not empty, only objects with at least one of the provided tags are rendered.
     * @return  ta render_result_t object.
     */
    render_result_t render_scene(vierkant::Rasterizer &renderer, const vierkant::SceneConstPtr &scene,
                                 const CameraPtr &cam, const std::set<std::string> &tags) override;

    std::vector<uint16_t> pick(const glm::vec2 & /*normalized_coord*/, const glm::vec2 & /*normalized_size*/) override
    {
        return {};
    };

private:
    explicit PhysicsDebugRenderer(const create_info_t &create_info);

    struct frame_context_t
    {
        vierkant::Framebuffer frame_buffer;
        vierkant::Semaphore semaphore;
        uint64_t current_semaphore_value = 0;
        settings_t settings = {};
    };
    std::vector<frame_context_t> m_frame_contexts;

    vierkant::DrawContext m_draw_context;
    vierkant::Rasterizer m_rasterizer;
    vierkant::PipelineCachePtr m_pipeline_cache;
    VkQueue m_queue = VK_NULL_HANDLE;
    std::unordered_map<vierkant::GeometryConstPtr, vierkant::MeshPtr> m_physics_meshes;
};
}// namespace vierkant
