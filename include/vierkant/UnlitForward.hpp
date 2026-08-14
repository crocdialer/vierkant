//
// Created by crocdialer on 6/15/20.
//

#pragma once

#include "vierkant/PipelineCache.hpp"
#include "vierkant/SceneRenderer.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(UnlitForward)

class UnlitForward : public vierkant::SceneRenderer
{
public:
    static UnlitForwardPtr create(const vierkant::DevicePtr &device);

    UnlitForward(const UnlitForward &) = delete;

    UnlitForward(UnlitForward &&) = delete;

    UnlitForward &operator=(UnlitForward other) = delete;

    /**
     * @brief   Render a scene with a provided camera.
     *
     * @param   renderer    a provided vierkant::Renderer.
     * @param   scene       the scene to render.
     * @param   cam         the camera to use.
     * @param   layer_mask  bitmask of vierkant::layer_t, only matching objects are rendered.
     * @return  ta render_result_t object.
     */
    render_result_t render_scene(vierkant::Rasterizer &renderer, const vierkant::SceneConstPtr &scene,
                                 const Object3DPtr &cam, uint32_t layer_mask) override;

    std::vector<uint16_t> pick(const glm::vec2 & /*normalized_coord*/, const glm::vec2 & /*normalized_size*/) override
    {
        return {};
    };

private:
    explicit UnlitForward(const vierkant::DevicePtr &device);

    vierkant::PipelineCachePtr m_pipeline_cache;
};

}// namespace vierkant
