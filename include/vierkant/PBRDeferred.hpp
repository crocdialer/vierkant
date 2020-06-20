//
// Created by crocdialer on 6/19/20.
//
#pragma once

#include "vierkant/PipelineCache.hpp"
#include "vierkant/SceneRenderer.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(PBRDeferred);

class PBRDeferred : public vierkant::SceneRenderer
{
public:

    static PBRDeferredPtr create(const vierkant::DevicePtr &device);

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
     * @return  the number of objects drawn
     */
    uint32_t render_scene(vierkant::Renderer &renderer,
                          const vierkant::SceneConstPtr &scene,
                          const CameraPtr &cam,
                          const std::set<std::string> &tags) override;

private:

    enum ShaderPropertyFlagBits
    {
        PROP_DEFAULT = 0x00,
        PROP_ALBEDO = 0x01,
        PROP_SKIN = 0x02,
        PROP_NORMAL = 0x04,
        PROP_SPEC = 0x08,
        PROP_AO_METAL_ROUGH = 0x10,
        PROP_EMMISION = 0x20
    };

    explicit PBRDeferred(const vierkant::DevicePtr &device);

    vierkant::PipelineCachePtr m_pipeline_cache;

    std::unordered_map<uint32_t, vierkant::shader_stage_map_t> m_shader_stages;
};

}// namespace vierkant


