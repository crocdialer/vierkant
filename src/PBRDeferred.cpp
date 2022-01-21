//
// Created by crocdialer on 6/19/20.
//

#include <crocore/gaussian.hpp>

#include <vierkant/cubemap_utils.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/culling.hpp>

#include <vierkant/PBRDeferred.hpp>

namespace vierkant
{

float halton(uint32_t index, int base)
{
    float f = 1;
    float r = 0;
    int current = index;

    do
    {
        f = f / base;
        r = r + f * (current % base);
        current = glm::floor(current / base);
    } while (current > 0);
    return r;
}

PBRDeferred::PBRDeferred(const DevicePtr &device, const create_info_t &create_info) :
        m_device(device)
{
    m_queue = create_info.queue ? create_info.queue : device->queue();

    m_pipeline_cache = create_info.pipeline_cache ?
                       create_info.pipeline_cache : vierkant::PipelineCache::create(device);

    m_frame_assets.resize(create_info.num_frames_in_flight);

    vierkant::Framebuffer::create_info_t post_fx_buffer_info = {};
    post_fx_buffer_info.size = create_info.size;
    post_fx_buffer_info.color_attachment_format.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    post_fx_buffer_info.color_attachment_format.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    vierkant::RenderPassPtr g_renderpass, lighting_renderpass, sky_renderpass, post_fx_renderpass;

    // create renderer for g-buffer-pass
    vierkant::Renderer::create_info_t post_render_info = {};
    post_render_info.num_frames_in_flight = create_info.num_frames_in_flight;
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport.width = static_cast<float>(create_info.size.width);
    post_render_info.viewport.height = static_cast<float>(create_info.size.height);
    post_render_info.viewport.maxDepth = 1;
    post_render_info.pipeline_cache = m_pipeline_cache;

    for(auto &asset : m_frame_assets)
    {
        asset.g_buffer = create_g_buffer(device, create_info.size, g_renderpass);
        asset.g_buffer_ubo = vierkant::Buffer::create(device, nullptr, sizeof(glm::vec2),
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        g_renderpass = asset.g_buffer.renderpass();

        // init lighting framebuffer
        vierkant::Framebuffer::AttachmentMap lighting_attachments, sky_attachments;
        {
            vierkant::Image::Format fmt = {};
            fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            fmt.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            fmt.extent = create_info.size;
            lighting_attachments[vierkant::Framebuffer::AttachmentType::Color] = {vierkant::Image::create(device, fmt)};

            sky_attachments = lighting_attachments;

            // use depth from g_buffer
            sky_attachments[vierkant::Framebuffer::AttachmentType::DepthStencil] = {
                    asset.g_buffer.depth_attachment()};

            lighting_renderpass = vierkant::Framebuffer::create_renderpass(device, lighting_attachments, true, false);
            sky_renderpass = vierkant::Framebuffer::create_renderpass(device, sky_attachments, false, false);
        }
        asset.lighting_buffer = vierkant::Framebuffer(device, lighting_attachments, lighting_renderpass);
        asset.lighting_ubo = vierkant::Buffer::create(device, nullptr, sizeof(environment_lighting_ubo_t),
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.sky_buffer = vierkant::Framebuffer(device, sky_attachments, sky_renderpass);

        // create post_fx ping pong buffers and renderers
        for(auto &post_fx_ping_pong : asset.post_fx_ping_pongs)
        {
            post_fx_ping_pong.framebuffer = vierkant::Framebuffer(device, post_fx_buffer_info, post_fx_renderpass);
            post_fx_ping_pong.framebuffer.clear_color = {{0.f, 0.f, 0.f, 0.f}};
            post_fx_ping_pong.renderer = vierkant::Renderer(device, post_render_info);
        }

        // create bloom
        Bloom::create_info_t bloom_info = {};
        bloom_info.size = create_info.size;
        bloom_info.size.width /= 2;
        bloom_info.size.height /= 2;
        bloom_info.num_blur_iterations = 3;
        asset.bloom = Bloom::create(device, bloom_info);

        asset.composition_ubo = vierkant::Buffer::create(device, nullptr, sizeof(composition_ubo_t),
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    // assign history
    for(uint32_t i = 0; i < m_frame_assets.size(); ++i)
    {
        uint32_t last_index = (i + m_frame_assets.size() - 1) % m_frame_assets.size();
        m_frame_assets[i].history_color = m_frame_assets[last_index].lighting_buffer.color_attachment();
        m_frame_assets[i].history_depth = m_frame_assets[last_index].g_buffer.depth_attachment();
    }

    // create renderer for g-buffer-pass
    vierkant::Renderer::create_info_t render_create_info = {};
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.sample_count = create_info.sample_count;
    render_create_info.viewport.width = static_cast<float>(create_info.size.width);
    render_create_info.viewport.height = static_cast<float>(create_info.size.height);
    render_create_info.viewport.maxDepth = 1;
    render_create_info.pipeline_cache = m_pipeline_cache;
    m_g_renderer = vierkant::Renderer(device, render_create_info);

    // create renderer for lighting-pass
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    m_light_renderer = vierkant::Renderer(device, render_create_info);
    m_sky_renderer = vierkant::Renderer(device, render_create_info);

    // create drawable for environment lighting-pass
    {
        graphics_pipeline_info_t fmt = {};
        fmt.depth_test = false;
        fmt.depth_write = false;

        // additive blending
        fmt.blend_state.blendEnable = true;
        fmt.blend_state.colorBlendOp = VK_BLEND_OP_ADD;
        fmt.blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        fmt.blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

        fmt.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
        fmt.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::pbr::lighting_environment_frag);
        fmt.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // descriptors
        vierkant::descriptor_t desc_ubo = {};
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        vierkant::descriptor_t desc_texture = {};
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        vierkant::descriptor_t desc_cubes = {};
        desc_cubes.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_cubes.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_drawable_lighting_env.descriptors[0] = desc_ubo;
        m_drawable_lighting_env.descriptors[1] = desc_texture;
        m_drawable_lighting_env.descriptors[2] = desc_cubes;
        m_drawable_lighting_env.num_vertices = 3;
        m_drawable_lighting_env.pipeline_format = fmt;
        m_drawable_lighting_env.use_own_buffers = true;
    }

    // create drawables for post-fx-pass
    {
        vierkant::Renderer::drawable_t fullscreen_drawable = {};

        fullscreen_drawable.num_vertices = 3;
        fullscreen_drawable.use_own_buffers = true;

        fullscreen_drawable.pipeline_format.depth_test = true;
        fullscreen_drawable.pipeline_format.depth_write = true;

        // same for all fullscreen passes
        fullscreen_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
        fullscreen_drawable.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // descriptor
        fullscreen_drawable.descriptors[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        fullscreen_drawable.descriptors[0].stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // TAA
        m_drawable_taa = fullscreen_drawable;
        m_drawable_taa.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::taa_frag);

        // TAA settings uniform-buffer
        vierkant::descriptor_t desc_taa_ubo = {};
        desc_taa_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_taa_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_taa_ubo.buffers = {vierkant::Buffer::create(m_device, nullptr, sizeof(taa_ubo_t),
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU)};

        m_drawable_taa.descriptors[1] = std::move(desc_taa_ubo);

        // fxaa
        m_drawable_fxaa = fullscreen_drawable;
        m_drawable_fxaa.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::fxaa_frag);

        // dof
        m_drawable_dof = fullscreen_drawable;
        m_drawable_dof.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::dof_frag);

        // DOF settings uniform-buffer
        vierkant::descriptor_t desc_dof_ubo = {};
        desc_dof_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_dof_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_dof_ubo.buffers = {vierkant::Buffer::create(m_device, &settings.dof, sizeof(postfx::dof_settings_t),
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU)};

        m_drawable_dof.descriptors[1] = std::move(desc_dof_ubo);

        // bloom
        m_drawable_bloom = fullscreen_drawable;
        m_drawable_bloom.pipeline_format.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_drawable_bloom.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::bloom_composition_frag);

        // composition ubo
        m_drawable_bloom.descriptors[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        m_drawable_bloom.descriptors[1].stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    // create required permutations of shader-stages
    m_g_buffer_shader_stages = vierkant::create_g_buffer_shader_stages(device);

    // use provided settings
    settings = create_info.settings;

    // bake BRDF into a lookup-table for image-based_lighting
    m_brdf_lut = create_BRDF_lut(device);

    // use provided convolutions
    m_conv_lambert = create_info.conv_lambert;
    m_conv_ggx = create_info.conv_ggx;

    m_draw_context = vierkant::DrawContext(device);

    // solid black color
    uint32_t v = 0x00000000;
    vierkant::Image::Format fmt;
    fmt.extent = {1, 1, 1};
    fmt.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_empty_img = vierkant::Image::create(m_device, &v, fmt);

    // populate a 2,3 halton sequence
    m_sample_offsets.resize(8);

    for(uint32_t i = 0; i < m_sample_offsets.size(); ++i)
    {
        m_sample_offsets[i] = glm::vec2(halton(i + 1, 2), halton(i + 1, 3));
    }
}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device, const create_info_t &create_info)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device, create_info));
}

SceneRenderer::render_result_t PBRDeferred::render_scene(Renderer &renderer,
                                                         const SceneConstPtr &scene,
                                                         const CameraPtr &cam,
                                                         const std::set<std::string> &tags)
{
    m_timestamp_current = std::chrono::steady_clock::now();

    auto cull_result = vierkant::cull(scene, cam, true, tags);

    // apply+update transform history
    update_matrix_history(cull_result);

    // create g-buffer
    auto &g_buffer = geometry_pass(cull_result);

    auto albedo_map = g_buffer.color_attachment(G_BUFFER_ALBEDO);

    // depth-attachment
    auto depth_map = g_buffer.depth_attachment();

    // default to color image
    auto out_img = albedo_map;

    // lighting-pass
    if(m_conv_lambert && m_conv_ggx)
    {
        auto &light_buffer = lighting_pass(cull_result);
        out_img = light_buffer.color_attachment(0);
    }

    // dof, bloom, anti-aliasing
    post_fx_pass(renderer, cam, out_img, depth_map);

    if(settings.draw_grid)
    {
        m_draw_context.draw_grid(renderer, 10.f, 100, cam->view_matrix(), cam->projection_matrix());
    }

    SceneRenderer::render_result_t ret = {};
    ret.num_objects = cull_result.drawables.size();
    m_timestamp_last = m_timestamp_current;
    return ret;
}

void PBRDeferred::update_matrix_history(vierkant::cull_result_t &cull_result)
{
    matrix_cache_t new_entry_matrix_cache;

    bone_buffer_cache_t new_bone_buffer_cache;

    for(const auto &mesh : cull_result.meshes)
    {
        vierkant::BufferPtr buffer;
        update_bone_uniform_buffer(mesh, buffer);

        new_bone_buffer_cache[mesh->root_bone] = buffer;
    }

    // insert previous matrices from cache, if any
    for(auto &drawable : cull_result.drawables)
    {
        // search previous matrices
        matrix_key_t key = {drawable.mesh, drawable.entry_index};
        auto it = m_entry_matrix_cache.find(key);
        if(it != m_entry_matrix_cache.end()){ drawable.last_matrices = it->second; }

        // previous matrices
        vierkant::descriptor_t desc_prev_matrices = {};
        desc_prev_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_prev_matrices.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        drawable.descriptors[Renderer::BINDING_PREVIOUS_MATRIX] = desc_prev_matrices;

        // descriptors for bone buffers, if necessary
        if(drawable.mesh && drawable.mesh->root_bone)
        {
            vierkant::descriptor_t desc_bones = {};
            desc_bones.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            desc_bones.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
            desc_bones.buffers = {new_bone_buffer_cache[drawable.mesh->root_bone]};
            drawable.descriptors[Renderer::BINDING_BONES] = desc_bones;

            // search previous bone-buffer
            auto prev_bones_it = m_bone_buffer_cache.find(drawable.mesh->root_bone);
            if(prev_bones_it != m_bone_buffer_cache.end()){ desc_bones.buffers = {prev_bones_it->second}; }
            drawable.descriptors[Renderer::BINDING_PREVIOUS_BONES] = desc_bones;
        }

        // store current matrices
        new_entry_matrix_cache[key] = drawable.matrices;
    }
    m_entry_matrix_cache = std::move(new_entry_matrix_cache);
    m_bone_buffer_cache = std::move(new_bone_buffer_cache);
}

vierkant::Framebuffer &PBRDeferred::geometry_pass(cull_result_t &cull_result)
{
    auto &frame_asset = m_frame_assets[m_g_renderer.current_index()];

    // jitter state
    float halton_multiplier = .2f;
    frame_asset.jitter_offset = settings.use_taa ? halton_multiplier * m_sample_offsets[m_sample_index] : glm::vec2(0);
    m_sample_index = (m_sample_index + 1) % m_sample_offsets.size();

    frame_asset.g_buffer_ubo->set_data(&frame_asset.jitter_offset, sizeof(glm::vec2));
    vierkant::descriptor_t jitter_desc = {};
    jitter_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    jitter_desc.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    jitter_desc.buffers = {frame_asset.g_buffer_ubo};

    // draw all gemoetry
    for(auto &drawable : cull_result.drawables)
    {
        uint32_t shader_flags = PROP_DEFAULT;

        // check if vertex-skinning is required
        if(drawable.mesh->root_bone){ shader_flags |= PROP_SKIN; }

        // check
        const auto &textures = drawable.mesh->materials[drawable.mesh->entries[drawable.entry_index].material_index]->textures;
        if(textures.count(vierkant::Material::Color)){ shader_flags |= PROP_ALBEDO; }
        if(textures.count(vierkant::Material::Normal)){ shader_flags |= PROP_NORMAL; }
        if(textures.count(vierkant::Material::Specular)){ shader_flags |= PROP_SPEC; }
        if(textures.count(vierkant::Material::Emission)){ shader_flags |= PROP_EMMISION; }
        if(textures.count(vierkant::Material::Ao_rough_metal)){ shader_flags |= PROP_AO_METAL_ROUGH; }

        // select shader-stages from cache
        auto stage_it = m_g_buffer_shader_stages.find(shader_flags);

        // fallback to default if not found
        if(stage_it != m_g_buffer_shader_stages.end()){ drawable.pipeline_format.shader_stages = stage_it->second; }
        else{ drawable.pipeline_format.shader_stages = m_g_buffer_shader_stages[PROP_DEFAULT]; }

        // set attachment count
        drawable.pipeline_format.attachment_count = G_BUFFER_SIZE;

        // add descriptor for a jitter-offset
        drawable.descriptors[Renderer::BINDING_JITTER_OFFSET] = jitter_desc;

        // stage drawable
        m_g_renderer.stage_drawable(std::move(drawable));
    }
    // material override
    m_g_renderer.disable_material = settings.disable_material;

    auto &g_buffer = m_frame_assets[m_g_renderer.current_index()].g_buffer;
    auto cmd_buffer = m_g_renderer.render(g_buffer);
    g_buffer.submit({cmd_buffer}, m_queue);
    return g_buffer;
}

vierkant::Framebuffer &PBRDeferred::lighting_pass(const cull_result_t &cull_result)
{
    // lighting-pass
    // |- IBL, environment lighting-pass + emission
    // |- TODO: draw light volumes with fancy stencil settings

    size_t index = (m_g_renderer.current_index() + m_g_renderer.num_indices() - 1) % m_g_renderer.num_indices();
    auto &frame_assets = m_frame_assets[index];

    environment_lighting_ubo_t ubo = {};
    ubo.camera_transform = cull_result.camera->global_transform();
    ubo.inverse_projection = glm::inverse(cull_result.camera->projection_matrix());
    ubo.num_mip_levels = static_cast<int>(std::log2(m_conv_ggx->width()) + 1);

    frame_assets.lighting_ubo->set_data(&ubo, sizeof(ubo));

    // environment lighting-pass
    auto drawable = m_drawable_lighting_env;
    drawable.descriptors[0].buffers = {frame_assets.lighting_ubo};
    drawable.descriptors[1].image_samplers = {frame_assets.g_buffer.color_attachment(G_BUFFER_ALBEDO),
                                              frame_assets.g_buffer.color_attachment(G_BUFFER_NORMAL),
                                              frame_assets.g_buffer.color_attachment(G_BUFFER_EMISSION),
                                              frame_assets.g_buffer.color_attachment(G_BUFFER_AO_ROUGH_METAL),
                                              frame_assets.g_buffer.color_attachment(G_BUFFER_MOTION),
                                              frame_assets.g_buffer.depth_attachment(),
                                              m_brdf_lut};
    drawable.descriptors[2].image_samplers = {m_conv_lambert, m_conv_ggx};

    // stage, render, submit
    m_light_renderer.stage_drawable(drawable);
    auto cmd_buffer = m_light_renderer.render(frame_assets.lighting_buffer);
    frame_assets.lighting_buffer.submit({cmd_buffer}, m_queue);

    // skybox rendering
    if(settings.draw_skybox)
    {
        if(cull_result.scene->environment())
        {
            m_draw_context.draw_skybox(m_sky_renderer, cull_result.scene->environment(), cull_result.camera);
        }
    }
    cmd_buffer = m_sky_renderer.render(frame_assets.sky_buffer);
    frame_assets.sky_buffer.submit({cmd_buffer}, m_queue);

    return frame_assets.sky_buffer;
}

void PBRDeferred::post_fx_pass(vierkant::Renderer &renderer,
                               const CameraPtr &cam,
                               const vierkant::ImagePtr &color,
                               const vierkant::ImagePtr &depth)
{
    size_t frame_index = (m_g_renderer.current_index() + m_g_renderer.num_indices() - 1) % m_g_renderer.num_indices();
    auto &frame_assets = m_frame_assets[frame_index];

    size_t buffer_index = 0;
    vierkant::ImagePtr output_img = color;

    // get next set of pingpong assets, increment index
    auto pingpong_render = [&frame_assets, &buffer_index, queue = m_queue](
            Renderer::drawable_t &drawable) -> vierkant::ImagePtr
    {
        auto &pingpong = frame_assets.post_fx_ping_pongs[buffer_index];
        buffer_index = (buffer_index + 1) % frame_assets.post_fx_ping_pongs.size();
        pingpong.renderer.stage_drawable(drawable);
        auto cmd_buf = pingpong.renderer.render(pingpong.framebuffer);
        pingpong.framebuffer.submit({cmd_buf}, queue);
        return pingpong.framebuffer.color_attachment(0);
    };

    // TAA
    if(settings.use_taa)
    {
        auto drawable = m_drawable_taa;
        drawable.descriptors[0].image_samplers = {output_img,
                                                  depth,
                                                  frame_assets.g_buffer.color_attachment(G_BUFFER_MOTION),
                                                  frame_assets.history_color,
                                                  frame_assets.history_depth};

        // pass projection matrix (vierkant::Renderer will extract near-/far-clipping planes)
        drawable.matrices.projection = cam->projection_matrix();

        glm::mat4 current_vp_matrix = cam->projection_matrix() * cam->view_matrix();
        glm::mat4 jittered_vp_matrix = current_vp_matrix;
        jittered_vp_matrix[3].xy() += frame_assets.jitter_offset;

        glm::mat4 previous_view_projection = m_previous_view_projection ? *m_previous_view_projection : current_vp_matrix;
        m_previous_view_projection = current_vp_matrix;
        if(!drawable.descriptors[1].buffers.empty())
        {
            taa_ubo_t taa_ubo = {};
            taa_ubo.near = cam->near();
            taa_ubo.far = cam->far();
            taa_ubo.sample_offset = frame_assets.jitter_offset;
            taa_ubo.current_inverse_vp = glm::inverse(jittered_vp_matrix);
            taa_ubo.previous_vp = previous_view_projection;
            drawable.descriptors[1].buffers.front()->set_data(&taa_ubo, sizeof(taa_ubo_t));
        }
        output_img = pingpong_render(drawable);
    }

    // tonemap / bloom
    if(settings.tonemap)
    {
        auto bloom_img = m_empty_img;

        // generate bloom image
        if(settings.bloom){ bloom_img = frame_assets.bloom->apply(output_img, m_queue, {}); }

        // motionblur
        auto motion_img = m_empty_img;
        if(settings.motionblur){ motion_img = frame_assets.g_buffer.color_attachment(G_BUFFER_MOTION); }

        composition_ubo_t comp_ubo = {};
        comp_ubo.exposure = settings.exposure;
        comp_ubo.gamma = settings.gamma;

        using duration_t = std::chrono::duration<float>;
        comp_ubo.time_delta = duration_t(m_timestamp_current - m_timestamp_last).count();
        comp_ubo.motionblur_gain = settings.motionblur_gain;
//        comp_ubo.shutter_time = ;
//        comp_ubo.motionblur_gain = ;

        frame_assets.composition_ubo->set_data(&comp_ubo, sizeof(composition_ubo_t));

        m_drawable_bloom.descriptors[0].image_samplers = {output_img, bloom_img, motion_img};
        m_drawable_bloom.descriptors[1].buffers = {frame_assets.composition_ubo};

        output_img = pingpong_render(m_drawable_bloom);
    }

    // fxaa
    if(settings.use_fxaa)
    {
        auto drawable = m_drawable_fxaa;
        drawable.descriptors[0].image_samplers = {output_img};

        output_img = pingpong_render(drawable);
    }

    // depth of field
    if(settings.dof.enabled)
    {
        auto drawable = m_drawable_dof;
        drawable.descriptors[0].image_samplers = {output_img, depth};

        // pass projection matrix (vierkant::Renderer will extract near-/far-clipping planes)
        drawable.matrices.projection = cam->projection_matrix();

        if(!drawable.descriptors[1].buffers.empty())
        {
            drawable.descriptors[1].buffers.front()->set_data(&settings.dof, sizeof(postfx::dof_settings_t));
        }
        output_img = pingpong_render(drawable);
    }

    // draw final color+depth with provided renderer
    m_draw_context.draw_image_fullscreen(renderer, output_img, depth, true);
}

vierkant::ImagePtr PBRDeferred::create_BRDF_lut(const vierkant::DevicePtr &device)
{
    const glm::vec2 size(512);

    // framebuffer image-format
    vierkant::Image::Format img_fmt = {};
    img_fmt.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_fmt.format = VK_FORMAT_R16G16_SFLOAT;

    // create framebuffer
    vierkant::Framebuffer::create_info_t fb_create_info = {};
    fb_create_info.size = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    fb_create_info.color_attachment_format = img_fmt;

    auto framebuffer = vierkant::Framebuffer(device, fb_create_info);

    // render
    vierkant::Renderer::create_info_t render_create_info = {};
    render_create_info.num_frames_in_flight = 1;
    render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    render_create_info.viewport.width = static_cast<float>(framebuffer.extent().width);
    render_create_info.viewport.height = static_cast<float>(framebuffer.extent().height);
    render_create_info.viewport.maxDepth = static_cast<float>(framebuffer.extent().depth);
    auto renderer = vierkant::Renderer(device, render_create_info);

    // create a drawable
    vierkant::Renderer::drawable_t drawable = {};
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
    drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
            vierkant::create_shader_module(device, vierkant::shaders::pbr::brdf_lut_frag);

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

    return framebuffer.color_attachment(0);
}

void PBRDeferred::set_environment(const ImagePtr &cubemap)
{
    constexpr uint32_t lambert_size = 128;

    if(cubemap)
    {
        VkQueue queue = m_device->queues(vierkant::Device::Queue::GRAPHICS)[0];

        m_conv_lambert = vierkant::create_convolution_lambert(m_device, cubemap, lambert_size, queue);
        m_conv_ggx = vierkant::create_convolution_ggx(m_device, cubemap, cubemap->width(), queue);

        m_conv_lambert->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_conv_ggx->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

const vierkant::Framebuffer &PBRDeferred::g_buffer() const
{
    size_t last_index = (m_g_renderer.num_indices() + m_g_renderer.current_index() - 1) % m_g_renderer.num_indices();
    return m_frame_assets[last_index].g_buffer;
}

const vierkant::Framebuffer &PBRDeferred::lighting_buffer() const
{
    size_t last_index =
            (m_light_renderer.num_indices() + m_light_renderer.current_index() - 1) % m_light_renderer.num_indices();
    return m_frame_assets[last_index].lighting_buffer;
}

void PBRDeferred::set_environment(const ImagePtr &lambert, const ImagePtr &ggx)
{
    m_conv_lambert = lambert;
    m_conv_ggx = ggx;
}

size_t PBRDeferred::matrix_key_hash_t::operator()(PBRDeferred::matrix_key_t const &key) const
{
    size_t h = 0;
    crocore::hash_combine(h, key.mesh);
    crocore::hash_combine(h, key.entry_index);
    return h;
}

void PBRDeferred::update_bone_uniform_buffer(const vierkant::MeshConstPtr &mesh, vierkant::BufferPtr &out_buffer)
{
    if(mesh && mesh->root_bone && mesh->animation_index < mesh->node_animations.size())
    {
        std::vector<glm::mat4> bones_matrices;
        vierkant::nodes::build_node_matrices_bfs(mesh->root_bone, mesh->node_animations[mesh->animation_index],
                                                 bones_matrices);

        if(!out_buffer)
        {
            out_buffer = vierkant::Buffer::create(m_device, bones_matrices,
                                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
        else{ out_buffer->set_data(bones_matrices); }
    }
}

}// namespace vierkant