//
// Created by crocdialer on 6/15/20.
//

#include "vierkant/culling.hpp"
#include "vierkant/ForwardSceneRenderer.hpp"

namespace vierkant
{

uint32_t ForwardSceneRenderer::render_scene(vierkant::Renderer &renderer,
                                            const vierkant::SceneConstPtr &scene,
                                            const vierkant::CameraPtr &cam,
                                            const std::set<std::string> &tags)
{
    if(!m_pipeline_cache){ m_pipeline_cache = vierkant::PipelineCache::create(renderer.device()); }

    auto cull_result = vierkant::cull(scene, cam, tags);
    uint32_t num_drawables = cull_result.drawables.size();

    for(auto &drawable : cull_result.drawables)
    {
        vierkant::ShaderType shader_type;

        // check for presence of a color-texture
        auto it = drawable.descriptors.find(vierkant::Renderer::BINDING_TEXTURES);
        bool has_texture = it != drawable.descriptors.end() && !it->second.image_samplers.empty();

        // check if vertex-skinning is required
        bool has_bones = static_cast<bool>(drawable.mesh->root_bone);

        if(has_bones)
        {
            if(has_texture){ shader_type = vierkant::ShaderType::UNLIT_TEXTURE_SKIN; }
            else{ shader_type = vierkant::ShaderType::UNLIT_COLOR_SKIN; }
        }
        else
        {
            if(has_texture){ shader_type = vierkant::ShaderType::UNLIT_TEXTURE; }
            else{ shader_type = vierkant::ShaderType::UNLIT_COLOR; }
        }
        drawable.pipeline_format.shader_stages = m_pipeline_cache->shader_stages(shader_type);

        // stage drawable
        renderer.stage_drawable(std::move(drawable));
    }
    return num_drawables;
}

ForwardSceneRenderer::ForwardSceneRenderer(const vierkant::DevicePtr &device) :
        m_pipeline_cache(vierkant::PipelineCache::create(device))
{

}

ForwardSceneRendererPtr ForwardSceneRenderer::create(const vierkant::DevicePtr &device)
{
    return vierkant::ForwardSceneRendererPtr(new ForwardSceneRenderer(device));
}

}
