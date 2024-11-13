#pragma once

#include <vierkant/DrawContext.hpp>
#include <vierkant/SceneRenderer.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(PhysicsDebugRenderer)

class PhysicsDebugRenderer : public vierkant::SceneRenderer
{
public:
    static PhysicsDebugRendererPtr create(const vierkant::DevicePtr &device);

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
    explicit PhysicsDebugRenderer(const vierkant::DevicePtr &device);

    vierkant::DrawContext m_draw_context;
    vierkant::PipelineCachePtr m_pipeline_cache;
    std::unordered_map<vierkant::GeometryConstPtr, vierkant::MeshPtr> m_physics_meshes;
};
}// namespace vierkant
