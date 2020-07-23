//
// Created by crocdialer on 6/19/20.
//

#include <vierkant/cubemap_utils.hpp>
#include "vierkant/shaders.hpp"
#include "vierkant/culling.hpp"
#include "vierkant/PBRDeferred.hpp"

namespace vierkant
{

PBRDeferred::PBRDeferred(const DevicePtr &device, const create_info_t &create_info) :
        m_device(device)
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
    g_buffer_info.color_attachment_format.sample_count = create_info.sample_count;

    g_buffer_info.depth_attachment_format.format = VK_FORMAT_D32_SFLOAT;
    g_buffer_info.depth_attachment_format.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    g_buffer_info.depth_attachment_format.usage =
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    vierkant::RenderPassPtr g_renderpass;

    vierkant::RenderPassPtr lighting_renderpass;

    for(auto &asset : m_frame_assets)
    {
        asset.g_buffer = vierkant::Framebuffer(device, g_buffer_info, g_renderpass);
        asset.g_buffer.clear_color = {{0.f, 0.f, 0.f, 0.f}};
        g_renderpass = asset.g_buffer.renderpass();

        // init lighting framebuffer
        vierkant::Framebuffer::AttachmentMap lighting_attachments;
        {
            vierkant::Image::Format fmt = {};
            fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            fmt.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            fmt.extent = create_info.size;
            lighting_attachments[vierkant::Framebuffer::AttachmentType::Color] = {vierkant::Image::create(device, fmt)};

            // use depth from g_buffer
            lighting_attachments[vierkant::Framebuffer::AttachmentType::DepthStencil] = {
                    asset.g_buffer.depth_attachment()};
        }
        asset.lighting_buffer = vierkant::Framebuffer(device, lighting_attachments, lighting_renderpass);
        lighting_renderpass = asset.lighting_buffer.renderpass();
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

    // create required permutations of shader-stages
    create_shader_stages(device);

    // bake BRDF into a lookup-table for image-based_lighting
    m_brdf_lut = create_BRDF_lut(device);

    m_draw_context = vierkant::DrawContext(device);
}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device, const create_info_t &create_info)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device, create_info));
}

uint32_t PBRDeferred::render_scene(Renderer &renderer, const SceneConstPtr &scene, const CameraPtr &cam,
                                   const std::set<std::string> &tags)
{
    auto cull_result = vierkant::cull(scene, cam, true, tags);
    uint32_t num_drawables = cull_result.drawables.size();

    // create g-buffer
    auto &g_buffer = geometry_pass(cull_result);

    auto albedo_map = g_buffer.color_attachment(G_BUFFER_ALBEDO);

    // depth-attachment
    auto depth_map = g_buffer.depth_attachment();

    // lighting-pass
    // |- use g buffer
    // |- draw light volumes with fancy stencil settings
    // IBL, environment lighting-pass
    lighting_pass(cull_result);

    // dof, bloom

    // skybox
    // compositing, post-fx
    // |- use lighting buffer
    // |- stage fullscreen-draw of compositing-pass -> renderer
    m_draw_context.draw_image_fullscreen(renderer, albedo_map, depth_map);

    return num_drawables;
}

vierkant::Framebuffer &PBRDeferred::geometry_pass(cull_result_t &cull_result)
{
    // draw all gemoetry
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
        if(textures.count(vierkant::Material::Ao_rough_metal)){ shader_flags |= PROP_AO_METAL_ROUGH; }

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
    g_buffer.submit({cmd_buffer}, m_g_renderer.device()->queue());
    return g_buffer;
}

void PBRDeferred::create_shader_stages(const DevicePtr &device)
{
    // vertex
    auto pbr_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr_vert);
    auto pbr_skin_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr_skin_vert);
    auto pbr_tangent_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr_tangent_vert);
    auto pbr_tangent_skin_vert = vierkant::create_shader_module(device, vierkant::shaders::pbr_tangent_skin_vert);

    // fragment
    auto pbr_g_buffer_frag = vierkant::create_shader_module(device, vierkant::shaders::pbr_g_buffer_frag);
    auto pbr_g_buffer_albedo_frag = vierkant::create_shader_module(device, vierkant::shaders::pbr_g_buffer_albedo_frag);
    auto pbr_g_buffer_albedo_normal_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr_g_buffer_albedo_normal_frag);
    auto pbr_g_buffer_albedo_normal_rough_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr_g_buffer_albedo_normal_rough_frag);
    auto pbr_g_buffer_complete_frag =
            vierkant::create_shader_module(device, vierkant::shaders::pbr_g_buffer_complete_frag);

    auto &stages_default = m_shader_stages[PROP_DEFAULT];
    stages_default[VK_SHADER_STAGE_VERTEX_BIT] = pbr_vert;
    stages_default[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_frag;

    // albedo
    auto &stages_albedo = m_shader_stages[PROP_ALBEDO];
    stages_albedo[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_albedo[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_albedo_frag;

    // skin
    auto &stages_skin = m_shader_stages[PROP_SKIN];
    stages_skin[VK_SHADER_STAGE_VERTEX_BIT] = pbr_skin_vert;
    stages_skin[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_frag;

    // skin + albedo
    auto &stages_skin_color = m_shader_stages[PROP_SKIN | PROP_ALBEDO];
    stages_skin_color[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_skin_vert;
    stages_skin_color[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_albedo_frag;

    // albedo + normals
    auto &stages_color_normal = m_shader_stages[PROP_ALBEDO | PROP_NORMAL];
    stages_color_normal[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_color_normal[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_albedo_normal_frag;

    // albedo + normals + ao/rough/metal
    auto &stages_color_normal_rough = m_shader_stages[PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH];
    stages_color_normal_rough[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_color_normal_rough[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_albedo_normal_rough_frag;

    // skin + albedo + normals + ao/rough/metal
    auto &stages_skin_color_normal_rough = m_shader_stages[PROP_SKIN | PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH];
    stages_skin_color_normal_rough[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_skin_vert;
    stages_skin_color_normal_rough[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_albedo_normal_rough_frag;

    // albedo + normals + ao/rough/metal + emmission
    auto &stages_complete = m_shader_stages[PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH | PROP_EMMISION];
    stages_complete[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_vert;
    stages_complete[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_complete_frag;

    // skin + albedo + normals + ao/rough/metal + emmission
    auto &stages_skin_complete = m_shader_stages[PROP_SKIN | PROP_ALBEDO | PROP_NORMAL | PROP_AO_METAL_ROUGH |
                                                 PROP_EMMISION];
    stages_skin_complete[VK_SHADER_STAGE_VERTEX_BIT] = pbr_tangent_skin_vert;
    stages_skin_complete[VK_SHADER_STAGE_FRAGMENT_BIT] = pbr_g_buffer_complete_frag;
}

vierkant::ImagePtr PBRDeferred::create_BRDF_lut(const vierkant::DevicePtr &device)
{
    const glm::vec2 size(512);

    // framebuffer image-format
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_fmt.format = VK_FORMAT_R32G32_SFLOAT;

    // create cube framebuffer
    vierkant::Framebuffer::create_info_t fb_create_info = {};
    fb_create_info.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    fb_create_info.color_attachment_format = img_fmt;

    auto framebuffer = vierkant::Framebuffer(device, fb_create_info);

    // render
    vierkant::Renderer::create_info_t render_create_info = {};
    render_create_info.renderpass = framebuffer.renderpass();
    render_create_info.num_frames_in_flight = 1;
    render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    render_create_info.viewport.width = framebuffer.extent().width;
    render_create_info.viewport.height = framebuffer.extent().height;
    render_create_info.viewport.maxDepth = framebuffer.extent().depth;
    auto renderer = vierkant::Renderer(device, render_create_info);

    // create a drawable
    vierkant::Renderer::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::fullscreen_texture_vert);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::brdf_lut_frag);

    drawable.num_vertices = 3;

    drawable.pipeline_format.blend_state.blendEnable = false;
    drawable.pipeline_format.depth_test = false;
    drawable.pipeline_format.depth_write = false;
    drawable.use_own_buffers = true;

    // stage drawable
    renderer.stage_drawable(drawable);

    auto cmd_buf = renderer.render(framebuffer);
    auto fence = framebuffer.submit({cmd_buf}, device->queue());

    // mandatory to sync here
    vkWaitForFences(device->handle(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    auto attach_it = framebuffer.attachments().find(vierkant::Framebuffer::AttachmentType::Color);

    // return color-attachment
    if(attach_it != framebuffer.attachments().end() && !attach_it->second.empty()){ return attach_it->second.front(); }

    return vierkant::ImagePtr();
}

void PBRDeferred::set_environment(const ImagePtr &cubemap)
{
    m_conv_lambert = vierkant::create_convolution_lambert(m_device, cubemap, cubemap->width());
    m_conv_ggx = vierkant::create_convolution_ggx(m_device, cubemap, cubemap->width());
}

void PBRDeferred::lighting_pass(const cull_result_t &cull_result)
{

}

}// namespace vierkant