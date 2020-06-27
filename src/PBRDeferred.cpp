//
// Created by crocdialer on 6/19/20.
//

#include "vierkant/shaders.hpp"
#include "vierkant/culling.hpp"
#include "vierkant/PBRDeferred.hpp"

namespace vierkant
{

uint32_t PBRDeferred::render_scene(Renderer &renderer, const SceneConstPtr &scene, const CameraPtr &cam,
                                   const std::set<std::string> &tags)
{
    auto cull_result = vierkant::cull(scene, cam, true, tags);
    uint32_t num_drawables = cull_result.drawables.size();

    // create g-buffer
    // |- draw all gemoetry
    for(auto &drawable : cull_result.drawables)
    {
        uint32_t shader_flags = PROP_DEFAULT;

        // check if vertex-skinning is required
        if(drawable.mesh->root_bone){ shader_flags |= PROP_SKIN; }

        // check
        const auto &textures = drawable.mesh->materials[drawable.entry_index]->textures;
        if(textures.count(vierkant::Material::Color)){ shader_flags |= PROP_ALBEDO; }
        if(textures.count(vierkant::Material::Normal)){ shader_flags |= PROP_NORMAL; }
        if(textures.count(vierkant::Material::Specular)){ shader_flags |= PROP_SPEC; }
        if(textures.count(vierkant::Material::Emission)){ shader_flags |= PROP_EMMISION; }
        if(textures.count(vierkant::Material::Ao_roughness_metal)){ shader_flags |= PROP_AO_METAL_ROUGH; }

        // select shader-stages from cache
        auto stage_it = m_shader_stages.find(shader_flags);

        if(stage_it != m_shader_stages.end()){ drawable.pipeline_format.shader_stages = stage_it->second; }
        else{ drawable.pipeline_format.shader_stages = m_shader_stages[PROP_DEFAULT]; }

        // set attachment count
        drawable.pipeline_format.attachment_count = G_BUFFER_SIZE;

        // stage drawable
        m_g_renderer.stage_drawable(std::move(drawable));
    }
    auto &g_buffer = m_frame_assets[m_g_renderer.current_index()].g_buffer;
    auto cmd_buffer = m_g_renderer.render(g_buffer);
    g_buffer.submit({cmd_buffer}, renderer.device()->queue());

    // check for resolve-attachment, fallback to color-attachment
    auto attach_it = g_buffer.attachments().find(vierkant::Framebuffer::AttachmentType::Resolve);

    if(attach_it == g_buffer.attachments().end())
    {
        attach_it = g_buffer.attachments().find(vierkant::Framebuffer::AttachmentType::Color);
    }
    const auto &attachments = attach_it->second;

    auto albedo_map = attachments[G_BUFFER_ALBEDO];

    m_draw_context.draw_image_fullscreen(renderer, albedo_map);

//    g_buffer.attachments()[C]
    // lighting-pass
    // |- use g buffer
    // |- draw light volumes with fancy stencil settings

    // dof, bloom

    // skybox
    // compositing, post-fx
    // |- use lighting buffer
    // |- stage fullscreen-draw of compositing-pass -> renderer

    return num_drawables;
}

PBRDeferred::PBRDeferred(const DevicePtr &device, const create_info_t &create_info)
{
    m_pipeline_cache = create_info.pipeline_cache ?
                       create_info.pipeline_cache : vierkant::PipelineCache::create(device);

    m_frame_assets.resize(create_info.num_frames_in_flight);

    vierkant::Framebuffer::create_info_t g_buffer_info = {};
    g_buffer_info.size = create_info.size;
    g_buffer_info.depth = true;
    g_buffer_info.num_color_attachments = G_BUFFER_SIZE;
    g_buffer_info.color_attachment_format.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    g_buffer_info.color_attachment_format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vierkant::RenderPassPtr g_renderpass;

    vierkant::Framebuffer::create_info_t lighting_buffer_info = {};
    lighting_buffer_info.size = create_info.size;

    vierkant::Framebuffer::AttachmentType lighting_attachments;
//    lighting_attachments[vierkant::Framebuffer::AttachmentType::Color] =
    vierkant::Image::Format fmt = {};
    fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    fmt.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    vierkant::RenderPassPtr lighting_renderpass;

    for(auto &asset : m_frame_assets)
    {
        asset.g_buffer = vierkant::Framebuffer(device, g_buffer_info, g_renderpass);
        asset.g_buffer.clear_color = {{0.f, 0.f, 0.f, 0.f}};
        g_renderpass = asset.g_buffer.renderpass();

        // TODO: init lighting framebuffer
    }

    vierkant::Renderer::create_info_t render_create_info = {};
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.sample_count = create_info.sample_count;
    render_create_info.viewport.width = create_info.size.width;
    render_create_info.viewport.height = create_info.size.height;
    render_create_info.viewport.maxDepth = 1;
    render_create_info.pipeline_cache = m_pipeline_cache;
    render_create_info.renderpass = g_renderpass;
    m_g_renderer = vierkant::Renderer(device, render_create_info);

    create_shader_stages(device);

    m_draw_context = vierkant::DrawContext(device);
}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device, const create_info_t &create_info)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device, create_info));
}

void PBRDeferred::create_shader_stages(const DevicePtr &device)
{
    auto pbr_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr_vert);
    auto pbr_tangent_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr_tangent_vert);
    auto pbr_tangent_skin_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr_tangent_skin_vert);

    auto pbr_g_buffer_frag = vierkant::create_shader_module(device, vierkant::shaders::pbr_g_buffer_frag);
    auto pbr_g_buffer_albedo_frag = vierkant::create_shader_module(device, vierkant::shaders::pbr_g_buffer_albedo_frag);

    auto &stages_default = m_shader_stages[PROP_DEFAULT];
    stages_default[VK_SHADER_STAGE_VERTEX_BIT] = pbr_vert;
    stages_default[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_frag;

    // albedo
    auto &stages_albedo = m_shader_stages[PROP_ALBEDO];
    stages_albedo[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_albedo[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_albedo_frag;

    // skin + color
    auto &stages_skin_color = m_shader_stages[PROP_ALBEDO | PROP_SKIN];
    stages_skin_color[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_skin_vert;
    stages_skin_color[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_albedo_frag;
}

}// namespace vierkant