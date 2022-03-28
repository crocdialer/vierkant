//
// Created by crocdialer on 6/15/20.
//

#include "vierkant/culling.hpp"
#include "vierkant/UnlitForward.hpp"

namespace vierkant
{

SceneRenderer::render_result_t UnlitForward::render_scene(vierkant::Renderer &renderer,
                                                          const vierkant::SceneConstPtr &scene,
                                                          const vierkant::CameraPtr &cam,
                                                          const std::set<std::string> &tags)
{
    vierkant::cull_params_t cull_params = {};
    cull_params.scene = scene;
    cull_params.camera = cam;
    cull_params.tags = tags;
    cull_params.check_intersection = true;

    auto cull_result = vierkant::cull(cull_params);

    for(auto &drawable : cull_result.drawables)
    {
        vierkant::ShaderType shader_type;

        // check for presence of a color-texture
        auto it = drawable.descriptors.find(vierkant::Renderer::BINDING_TEXTURES);
        bool has_texture = it != drawable.descriptors.end() && !it->second.images.empty();

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
    }

    // stage drawable
    renderer.stage_drawables(std::move(cull_result.drawables));

    render_result_t ret = {};
    ret.num_draws = cull_result.drawables.size();
    return ret;
}

UnlitForward::UnlitForward(const vierkant::DevicePtr &device) :
        m_pipeline_cache(vierkant::PipelineCache::create(device))
{

}

UnlitForwardPtr UnlitForward::create(const vierkant::DevicePtr &device)
{
    return vierkant::UnlitForwardPtr(new UnlitForward(device));
}

}
