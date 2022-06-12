//
// Created by crocdialer on 6/19/20.
//

#include <crocore/gaussian.hpp>

#include <vierkant/cubemap_utils.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/culling.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/punctual_light.hpp>
#include <vierkant/PBRDeferred.hpp>

namespace vierkant
{

float halton(uint32_t index, uint32_t base)
{
    float f = 1;
    float r = 0;
    uint32_t current = index;

    while(current)
    {
        f = f / static_cast<float>(base);
        r = r + f * static_cast<float>(current % base);
        current /= base;
    }
    return r;
}

PBRDeferred::PBRDeferred(const DevicePtr &device, const create_info_t &create_info) :
        m_device(device)
{
    m_logger = create_info.logger_name.empty() ? spdlog::default_logger() : spdlog::get(create_info.logger_name);
    m_logger->debug("PBRDeferred initialized");

    m_queue = create_info.queue ? create_info.queue : device->queue();

    m_command_pool = vierkant::create_command_pool(m_device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    m_pipeline_cache = create_info.pipeline_cache ?
                       create_info.pipeline_cache : vierkant::PipelineCache::create(device);

    m_frame_assets.resize(create_info.num_frames_in_flight);

    for(auto &asset: m_frame_assets)
    {
        resize_storage(asset, create_info.settings.resolution);

        asset.g_buffer_camera_ubo = vierkant::Buffer::create(m_device, nullptr, sizeof(glm::vec2),
                                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.lighting_param_ubo = vierkant::Buffer::create(device, nullptr, sizeof(environment_lighting_ubo_t),
                                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                            VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.lights_ubo = vierkant::Buffer::create(device, nullptr, sizeof(vierkant::lightsource_ubo_t),
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.composition_ubo = vierkant::Buffer::create(device, nullptr, sizeof(composition_ubo_t),
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    // create renderer for g-buffer-pass
    vierkant::Renderer::create_info_t render_create_info = {};
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.sample_count = create_info.sample_count;
    render_create_info.viewport.width = static_cast<float>(create_info.settings.resolution.x);
    render_create_info.viewport.height = static_cast<float>(create_info.settings.resolution.y);
    render_create_info.pipeline_cache = m_pipeline_cache;
    render_create_info.indirect_draw = true;
    m_g_renderer_pre = vierkant::Renderer(device, render_create_info);
    m_g_renderer_post = vierkant::Renderer(device, render_create_info);

    // create renderer for lighting-pass
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    render_create_info.indirect_draw = false;
    m_light_renderer = vierkant::Renderer(device, render_create_info);
    m_sky_renderer = vierkant::Renderer(device, render_create_info);
    m_taa_renderer = vierkant::Renderer(device, render_create_info);

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

        vierkant::descriptor_t desc_lights = {};
        desc_lights.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_lights.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_drawable_lighting_env.descriptors[0] = desc_ubo;
        m_drawable_lighting_env.descriptors[1] = desc_texture;
        m_drawable_lighting_env.descriptors[2] = desc_cubes;
        m_drawable_lighting_env.descriptors[3] = desc_lights;
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
        desc_taa_ubo.buffers = {vierkant::Buffer::create(m_device, nullptr, sizeof(camera_params_t),
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
        desc_dof_ubo.buffers = {vierkant::Buffer::create(m_device, &settings.dof, sizeof(vierkant::dof_settings_t),
                                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU)};

        m_drawable_dof.descriptors[1] = std::move(desc_dof_ubo);

        // bloom
        m_drawable_bloom = fullscreen_drawable;
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
    m_brdf_lut = create_info.brdf_lut ? create_info.brdf_lut : create_BRDF_lut(device, m_queue);

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

    // depth pyramid compute
    auto shader_stage = vierkant::create_shader_module(m_device, vierkant::shaders::pbr::depth_min_reduce_comp,
                                                       &m_depth_pyramid_local_size);
    m_depth_pyramid_computable.pipeline_info.shader_stage = shader_stage;

    // indirect-draw cull compute
    auto cull_shader_stage = vierkant::create_shader_module(m_device, vierkant::shaders::pbr::indirect_cull_comp,
                                                            &m_cull_compute_local_size);
    m_cull_computable.pipeline_info.shader_stage = cull_shader_stage;
}

PBRDeferred::~PBRDeferred()
{
    for(auto &frame_asset: m_frame_assets){ frame_asset.timeline.wait(frame_asset.semaphore_value_done); }
}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device, const create_info_t &create_info)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device, create_info));
}

void PBRDeferred::update_recycling(const SceneConstPtr &scene,
                                   const CameraPtr &cam,
                                   frame_assets_t &frame_asset) const
{
    vierkant::SelectVisitor<vierkant::MeshNode> mesh_visitor;
    scene->root()->accept(mesh_visitor);
    std::unordered_set<vierkant::MeshConstPtr> meshes;

    bool static_scene = true;
    bool materials_unchanged = true;
    bool scene_unchanged = true;

    size_t scene_hash = 0;

    for(const auto &n: mesh_visitor.objects)
    {
        meshes.insert(n->mesh);
        if(!n->mesh->node_animations.empty()){ static_scene = false; }
        crocore::hash_combine(scene_hash, n->transform());
    }
    if(scene_hash != frame_asset.scene_hash)
    {
        scene_unchanged = false;
        frame_asset.scene_hash = scene_hash;
    }

    for(const auto &mesh: meshes)
    {
        for(const auto &mat: mesh->materials)
        {
            auto h = mat->hash();
            if(frame_asset.material_hashes[mat] != h){ materials_unchanged = false; }
            frame_asset.material_hashes[mat] = h;
        }
    }
    bool need_culling = frame_asset.cull_result.camera != cam || meshes != frame_asset.cull_result.meshes;
    frame_asset.recycle_commands = static_scene && scene_unchanged && materials_unchanged && !need_culling;

    frame_asset.recycle_commands = frame_asset.recycle_commands && frame_asset.settings == settings;
    frame_asset.settings = settings;
}

SceneRenderer::render_result_t PBRDeferred::render_scene(Renderer &renderer,
                                                         const SceneConstPtr &scene,
                                                         const CameraPtr &cam,
                                                         const std::set<std::string> &tags)
{
    m_timestamp_current = std::chrono::steady_clock::now();

    // reference to current frame-assets
    auto &frame_asset = m_frame_assets[m_g_renderer_pre.current_index()];

    update_recycling(scene, cam, frame_asset);

    // TODO: optimization is not always playing nice
//    frame_asset.recycle_commands = false;

    if(!frame_asset.recycle_commands)
    {
        vierkant::cull_params_t cull_params = {};
        cull_params.scene = scene;
        cull_params.camera = cam;
        cull_params.tags = tags;
        cull_params.check_intersection = false;
        cull_params.world_space = true;
        frame_asset.cull_result = vierkant::cull(cull_params);
    }

    // timeline semaphore
    frame_asset.timeline.wait(frame_asset.semaphore_value_done);
    frame_asset.timeline = vierkant::Semaphore(m_device);

    resize_storage(frame_asset, settings.resolution);

    // apply+update transform history
    update_matrix_history(frame_asset);

    // create g-buffer
    auto &g_buffer = geometry_pass(frame_asset.cull_result);
    auto albedo_map = g_buffer.color_attachment(G_BUFFER_ALBEDO);

    // default to color image
    auto out_img = albedo_map;

    // lighting-pass
    if(m_conv_lambert && m_conv_ggx)
    {
        auto &light_buffer = lighting_pass(frame_asset.cull_result);
        out_img = light_buffer.color_attachment(0);
    }

    // dof, bloom, anti-aliasing
    post_fx_pass(renderer, cam, out_img, frame_asset.depth_map);

    draw_cull_result_t gpu_cull_result = {};

    if(frame_asset.cull_result_buffer_host)
    {
        gpu_cull_result = *reinterpret_cast<draw_cull_result_t *>(frame_asset.cull_result_buffer_host->map());
    }

    SceneRenderer::render_result_t ret = {};
    ret.num_draws = frame_asset.cull_result.drawables.size();
    ret.num_frustum_culled = gpu_cull_result.num_frustum_culled;
    ret.num_occlusion_culled = gpu_cull_result.num_occlusion_culled;
    ret.num_distance_culled = gpu_cull_result.num_distance_culled;

    vierkant::semaphore_submit_info_t semaphore_submit_info = {};
    semaphore_submit_info.semaphore = frame_asset.timeline.handle();
    semaphore_submit_info.wait_value = frame_asset.semaphore_value_done;
    semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    ret.semaphore_infos = {semaphore_submit_info};

    m_timestamp_last = m_timestamp_current;
    return ret;
}

void PBRDeferred::update_matrix_history(frame_assets_t &frame_asset)
{
    using bone_offset_cache_t = std::unordered_map<vierkant::nodes::NodeConstPtr, size_t>;
    matrix_cache_t new_entry_matrix_cache;

    bone_offset_cache_t bone_buffer_cache;
    std::vector<glm::mat4> all_bones_matrices;

    size_t last_index =
            (m_g_renderer_pre.current_index() + m_g_renderer_pre.num_indices() - 1) % m_g_renderer_pre.num_indices();
    auto &last_frame_asset = m_frame_assets[last_index];

    for(const auto &mesh: frame_asset.cull_result.meshes)
    {
        if(mesh && mesh->root_bone && mesh->animation_index < mesh->node_animations.size())
        {
            std::vector<glm::mat4> bones_matrices;
            vierkant::nodes::build_node_matrices_bfs(mesh->root_bone, mesh->node_animations[mesh->animation_index],
                                                     bones_matrices);

            // keep track of offset
            bone_buffer_cache[mesh->root_bone] = all_bones_matrices.size() * sizeof(glm::mat4);
            all_bones_matrices.insert(all_bones_matrices.end(), bones_matrices.begin(), bones_matrices.end());
        }
    }

    if(!frame_asset.bone_buffer)
    {
        frame_asset.bone_buffer = vierkant::Buffer::create(m_device, all_bones_matrices,
                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                           VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    else{ frame_asset.bone_buffer->set_data(all_bones_matrices); }

    if(!frame_asset.recycle_commands)
    {
        // insert previous matrices from cache, if any
        for(auto &drawable: frame_asset.cull_result.drawables)
        {
            // search previous matrices
            matrix_key_t key = {drawable.mesh, drawable.entry_index};
            auto it = m_entry_matrix_cache.find(key);
            if(it != m_entry_matrix_cache.end()){ drawable.last_matrices = it->second; }

            // previous matrices
            vierkant::descriptor_t desc_prev_matrices = {};
            desc_prev_matrices.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            desc_prev_matrices.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
            drawable.descriptors[Renderer::BINDING_PREVIOUS_MATRIX] = desc_prev_matrices;

            // descriptors for bone buffers, if necessary
            if(drawable.mesh && drawable.mesh->root_bone)
            {
                vierkant::descriptor_t desc_bones = {};
                desc_bones.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_bones.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
                desc_bones.buffers = {frame_asset.bone_buffer};
                desc_bones.buffer_offsets = {bone_buffer_cache[drawable.mesh->root_bone]};
                drawable.descriptors[Renderer::BINDING_BONES] = desc_bones;

                if(last_frame_asset.bone_buffer && last_frame_asset.bone_buffer->num_bytes() ==
                                                   frame_asset.bone_buffer->num_bytes())
                {
                    desc_bones.buffers = {last_frame_asset.bone_buffer};
                }
                drawable.descriptors[Renderer::BINDING_PREVIOUS_BONES] = desc_bones;
            }

            // store current matrices
            new_entry_matrix_cache[key] = drawable.matrices;
        }
        m_entry_matrix_cache = std::move(new_entry_matrix_cache);
    }
}

vierkant::Framebuffer &PBRDeferred::geometry_pass(cull_result_t &cull_result)
{
    auto &frame_asset = m_frame_assets[m_g_renderer_pre.current_index()];

    size_t last_index =
            (m_g_renderer_pre.current_index() + m_g_renderer_pre.num_indices() - 1) % m_g_renderer_pre.num_indices();
    auto &last_frame_asset = m_frame_assets[last_index];

    // jitter state
    constexpr float halton_multiplier = 1.f;
    const glm::vec2 pixel_step =
            1.f / glm::vec2(frame_asset.g_buffer_post.extent().width, frame_asset.g_buffer_post.extent().height);

    glm::vec2 jitter_offset = halton_multiplier * pixel_step * (m_sample_offsets[m_sample_index] - glm::vec2(.5f));
    jitter_offset = settings.use_taa ? jitter_offset : glm::vec2(0);
    m_sample_index = (m_sample_index + 1) % m_sample_offsets.size();

    // update camera/jitter ubo
    frame_asset.camera_params = {};
    frame_asset.camera_params.view = cull_result.camera->view_matrix();
    frame_asset.camera_params.projection = cull_result.camera->projection_matrix();
    frame_asset.camera_params.sample_offset = jitter_offset;
    frame_asset.camera_params.near = cull_result.camera->near();
    frame_asset.camera_params.far = cull_result.camera->far();

    camera_params_t cameras[2] = {frame_asset.camera_params, last_frame_asset.camera_params};
    frame_asset.g_buffer_camera_ubo->set_data(&cameras, sizeof(cameras));

    // decide on indirect rendering-path
    bool use_indrect_draw = cull_result.drawables.size() >= settings.draw_indrect_object_thresh;
    if(use_indrect_draw && (!m_g_renderer_pre.indirect_draw || !m_g_renderer_post.indirect_draw))
    {
        frame_asset.recycle_commands = false;
    }

    if(!frame_asset.recycle_commands)
    {
        vierkant::descriptor_t camera_desc = {};
        camera_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_desc.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        camera_desc.buffers = {frame_asset.g_buffer_camera_ubo};

        // draw all geometry
        for(auto &drawable: cull_result.drawables)
        {
            uint32_t shader_flags = PROP_DEFAULT;

            // check if vertex-skinning is required
            if(drawable.mesh->root_bone){ shader_flags |= PROP_SKIN; }

            // check if tangents are available
            if(drawable.mesh->vertex_attribs.count(Mesh::ATTRIB_TANGENT))
            {
                shader_flags |= PROP_TANGENT_SPACE;

                if(frame_asset.settings.tesselation)
                {
                    shader_flags |= PROP_TESSELATION;
                    drawable.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
                    drawable.pipeline_format.num_patch_control_points = 3;
                    drawable.descriptors[Renderer::BINDING_MATRIX].stage_flags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                    drawable.descriptors[Renderer::BINDING_PREVIOUS_MATRIX].stage_flags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                    camera_desc.stage_flags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                }
            }

            // select shader-stages from cache
            auto stage_it = m_g_buffer_shader_stages.find(shader_flags);

            // fallback to default if not found
            if(stage_it != m_g_buffer_shader_stages.end()){ drawable.pipeline_format.shader_stages = stage_it->second; }
            else{ drawable.pipeline_format.shader_stages = m_g_buffer_shader_stages[PROP_DEFAULT]; }

            // set attachment count
            drawable.pipeline_format.attachment_count = G_BUFFER_SIZE;

            // optional wireframe rendering
            drawable.pipeline_format.polygon_mode = frame_asset.settings.wireframe ? VK_POLYGON_MODE_LINE
                                                                                   : VK_POLYGON_MODE_FILL;

            // add descriptor for a jitter-offset
            drawable.descriptors[Renderer::BINDING_JITTER_OFFSET] = camera_desc;
        }
        // stage drawables
        m_g_renderer_pre.stage_drawables(cull_result.drawables);
        if(use_indrect_draw){ m_g_renderer_post.stage_drawables(cull_result.drawables); }

        m_g_renderer_pre.draw_indirect_delegate = {};
        m_g_renderer_post.draw_indirect_delegate = {};
    }

    // material override
    m_g_renderer_pre.disable_material = frame_asset.settings.disable_material;
    m_g_renderer_post.disable_material = frame_asset.settings.disable_material;

    m_g_renderer_pre.indirect_draw = use_indrect_draw;
    m_g_renderer_post.indirect_draw = use_indrect_draw;

    // draw last visible objects
    m_g_renderer_pre.draw_indirect_delegate = [this, &frame_asset](Renderer::indirect_draw_params_t &params)
    {
        frame_asset.clear_cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
        frame_asset.clear_cmd_buffer.begin();
        resize_indirect_draw_buffers(params.num_draws, frame_asset.indirect_draw_params_pre,
                                     frame_asset.clear_cmd_buffer.handle());

        if(params.num_draws && !frame_asset.recycle_commands)
        {
            params.draws_in->copy_to(frame_asset.indirect_draw_params_pre.draws_in,
                                     frame_asset.clear_cmd_buffer.handle());
        }

        frame_asset.clear_cmd_buffer.submit(m_queue);

        params.draws_out = frame_asset.indirect_draw_params_pre.draws_out;
        params.draws_counts_out = frame_asset.indirect_draw_params_pre.draws_counts_out;
    };

    // pre-render will repeat all previous draw-calls
    auto cmd_buffer_pre = m_g_renderer_pre.render(frame_asset.g_buffer_pre, frame_asset.recycle_commands);
    vierkant::semaphore_submit_info_t g_buffer_semaphore_submit_info_pre = {};
    g_buffer_semaphore_submit_info_pre.semaphore = frame_asset.timeline.handle();
    g_buffer_semaphore_submit_info_pre.signal_value = use_indrect_draw ? SemaphoreValue::G_BUFFER_LAST_VISIBLE
                                                                       : SemaphoreValue::G_BUFFER_ALL;
    frame_asset.g_buffer_pre.submit({cmd_buffer_pre}, m_queue,
                                    {g_buffer_semaphore_submit_info_pre});

    // depth-attachment
    frame_asset.depth_map = frame_asset.g_buffer_pre.depth_attachment();

    if(use_indrect_draw)
    {
        // generate depth-pyramid
        create_depth_pyramid(frame_asset);

        // post-render will perform actual culling
        m_g_renderer_post.draw_indirect_delegate = [this, cam = cull_result.camera, &frame_asset]
                (Renderer::indirect_draw_params_t &params)
        {
            resize_indirect_draw_buffers(params.num_draws, frame_asset.indirect_draw_params_post);

            cull_draw_commands(frame_asset,
                               cam,
                               frame_asset.depth_pyramid,
                               frame_asset.indirect_draw_params_pre.draws_in,
                               params.num_draws,
                               frame_asset.indirect_draw_params_pre.draws_out,
                               frame_asset.indirect_draw_params_pre.draws_counts_out,
                               frame_asset.indirect_draw_params_post.draws_out,
                               frame_asset.indirect_draw_params_post.draws_counts_out);

            params = frame_asset.indirect_draw_params_post;
        };

        vierkant::semaphore_submit_info_t g_buffer_semaphore_submit_info = {};
        g_buffer_semaphore_submit_info.semaphore = frame_asset.timeline.handle();
        g_buffer_semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        g_buffer_semaphore_submit_info.wait_value = SemaphoreValue::CULLING;
        g_buffer_semaphore_submit_info.signal_value = SemaphoreValue::G_BUFFER_ALL;
        auto cmd_buffer = m_g_renderer_post.render(frame_asset.g_buffer_post, frame_asset.recycle_commands);
        frame_asset.g_buffer_post.submit({cmd_buffer}, m_queue, {g_buffer_semaphore_submit_info});
    }

    frame_asset.semaphore_value_done = SemaphoreValue::G_BUFFER_ALL;
    return frame_asset.g_buffer_post;
}

vierkant::Framebuffer &PBRDeferred::lighting_pass(const cull_result_t &cull_result)
{
    // lighting-pass
    // |- IBL, environment lighting-pass + emission
    // |- TODO: draw light volumes with fancy stencil settings

    size_t index =
            (m_g_renderer_pre.current_index() + m_g_renderer_pre.num_indices() - 1) % m_g_renderer_pre.num_indices();
    auto &frame_asset = m_frame_assets[index];

    environment_lighting_ubo_t ubo = {};
    ubo.camera_transform = cull_result.camera->global_transform();
    ubo.inverse_projection = glm::inverse(cull_result.camera->projection_matrix());
    ubo.num_mip_levels = static_cast<int>(std::log2(m_conv_ggx->width()) + 1);
    ubo.environment_factor = frame_asset.settings.environment_factor;
    ubo.num_lights = 1;
    frame_asset.lighting_param_ubo->set_data(&ubo, sizeof(ubo));

//    // test lightsource
//    vierkant::lightsource_t l = {};
//    l.type = vierkant::LightType::Directional;
//    l.intensity = 2.f;

    std::vector<lightsource_ubo_t> lights_ubo; //= {vierkant::convert_light(l)};
    frame_asset.lights_ubo->set_data(lights_ubo);

    // environment lighting-pass
    auto drawable = m_drawable_lighting_env;
    drawable.descriptors[0].buffers = {frame_asset.lighting_param_ubo};
    drawable.descriptors[1].images = {frame_asset.g_buffer_post.color_attachment(G_BUFFER_ALBEDO),
                                      frame_asset.g_buffer_post.color_attachment(G_BUFFER_NORMAL),
                                      frame_asset.g_buffer_post.color_attachment(G_BUFFER_EMISSION),
                                      frame_asset.g_buffer_post.color_attachment(G_BUFFER_AO_ROUGH_METAL),
                                      frame_asset.g_buffer_post.color_attachment(G_BUFFER_MOTION),
                                      frame_asset.g_buffer_post.depth_attachment(),
                                      m_brdf_lut};
    drawable.descriptors[2].images = {m_conv_lambert, m_conv_ggx};
    drawable.descriptors[3].buffers = {frame_asset.lights_ubo};

    std::vector<VkCommandBuffer> primary_cmd_buffers;

    // stage, render, submit
    m_light_renderer.stage_drawable(drawable);
    auto cmd_buffer = m_light_renderer.render(frame_asset.lighting_buffer);
    primary_cmd_buffers.push_back(frame_asset.lighting_buffer.record_commandbuffer({cmd_buffer}));

    // skybox rendering
    if(frame_asset.settings.draw_skybox)
    {
        if(cull_result.scene->environment())
        {
            m_draw_context.draw_skybox(m_sky_renderer, cull_result.scene->environment(), cull_result.camera);
            cmd_buffer = m_sky_renderer.render(frame_asset.sky_buffer);
            primary_cmd_buffers.push_back(frame_asset.sky_buffer.record_commandbuffer({cmd_buffer}));
        }
    }
    frame_asset.semaphore_value_done = SemaphoreValue::LIGHTING;

    vierkant::semaphore_submit_info_t lighting_semaphore_info = {};
    lighting_semaphore_info.semaphore = frame_asset.timeline.handle();
    lighting_semaphore_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    lighting_semaphore_info.wait_value = SemaphoreValue::G_BUFFER_ALL;
    lighting_semaphore_info.signal_value = SemaphoreValue::LIGHTING;
    vierkant::submit(m_device, m_queue, primary_cmd_buffers, false, VK_NULL_HANDLE, {lighting_semaphore_info});
    return frame_asset.sky_buffer;
}

void PBRDeferred::post_fx_pass(vierkant::Renderer &renderer,
                               const CameraPtr &cam,
                               const vierkant::ImagePtr &color,
                               const vierkant::ImagePtr &depth)
{
    size_t frame_index =
            (m_g_renderer_pre.current_index() + m_g_renderer_pre.num_indices() - 1) % m_g_renderer_pre.num_indices();
    auto &frame_asset = m_frame_assets[frame_index];

    size_t buffer_index = 0;
    vierkant::ImagePtr output_img = color;

    // get next set of pingpong assets, increment index
    auto pingpong_render = [&frame_asset, &buffer_index, queue = m_queue](
            Renderer::drawable_t &drawable,
            const std::vector<vierkant::semaphore_submit_info_t> &semaphore_submit_infos = {}) -> vierkant::ImagePtr
    {
        auto &pingpong = frame_asset.post_fx_ping_pongs[buffer_index];
        buffer_index = (buffer_index + 1) % frame_asset.post_fx_ping_pongs.size();
        pingpong.renderer.stage_drawable(drawable);
        auto cmd_buf = pingpong.renderer.render(pingpong.framebuffer);
        pingpong.framebuffer.submit({cmd_buf}, queue, semaphore_submit_infos);
        return pingpong.framebuffer.color_attachment(0);
    };

    // TAA
    if(frame_asset.settings.use_taa)
    {
        size_t last_frame_index = (frame_index + m_g_renderer_pre.num_indices() - 1) % m_g_renderer_pre.num_indices();

        // assign history
        auto history_color = m_frame_assets[last_frame_index].taa_buffer.color_attachment();
        auto history_depth = m_frame_assets[last_frame_index].g_buffer_post.depth_attachment();

        auto drawable = m_drawable_taa;
        drawable.descriptors[0].images = {output_img,
                                          depth,
                                          frame_asset.g_buffer_post.color_attachment(G_BUFFER_MOTION),
                                          history_color,
                                          history_depth};

        if(!drawable.descriptors[1].buffers.empty())
        {
            drawable.descriptors[1].buffers.front()->set_data(&frame_asset.camera_params, sizeof(camera_params_t));
        }
        m_taa_renderer.stage_drawable(drawable);

        auto cmd = m_taa_renderer.render(frame_asset.taa_buffer);

        vierkant::semaphore_submit_info_t taa_semaphore_info = {};
        taa_semaphore_info.semaphore = frame_asset.timeline.handle();
        taa_semaphore_info.signal_value = SemaphoreValue::TAA;
        frame_asset.taa_buffer.submit({cmd}, m_queue, {taa_semaphore_info});
        frame_asset.semaphore_value_done = SemaphoreValue::TAA;
        output_img = frame_asset.taa_buffer.color_attachment();
    }

    // tonemap / bloom
    if(frame_asset.settings.tonemap)
    {
        auto bloom_img = m_empty_img;

        // generate bloom image
        if(frame_asset.settings.bloom){ bloom_img = frame_asset.bloom->apply(output_img, m_queue, {}); }

        // motionblur
        auto motion_img = m_empty_img;
        if(frame_asset.settings.motionblur){ motion_img = frame_asset.g_buffer_post.color_attachment(G_BUFFER_MOTION); }

        composition_ubo_t comp_ubo = {};
        comp_ubo.exposure = frame_asset.settings.exposure;
        comp_ubo.gamma = frame_asset.settings.gamma;

        using duration_t = std::chrono::duration<float>;
        comp_ubo.time_delta = duration_t(m_timestamp_current - m_timestamp_last).count();
        comp_ubo.motionblur_gain = frame_asset.settings.motionblur_gain;

        frame_asset.composition_ubo->set_data(&comp_ubo, sizeof(composition_ubo_t));

        m_drawable_bloom.descriptors[0].images = {output_img, bloom_img, motion_img};
        m_drawable_bloom.descriptors[1].buffers = {frame_asset.composition_ubo};

        vierkant::semaphore_submit_info_t tonemap_semaphore_info = {};
        tonemap_semaphore_info.semaphore = frame_asset.timeline.handle();
        tonemap_semaphore_info.signal_value = SemaphoreValue::TONEMAP;
        output_img = pingpong_render(m_drawable_bloom, {tonemap_semaphore_info});
        frame_asset.semaphore_value_done = SemaphoreValue::TONEMAP;
    }

    // fxaa
    if(frame_asset.settings.use_fxaa)
    {
        auto drawable = m_drawable_fxaa;
        drawable.descriptors[0].images = {output_img};

        output_img = pingpong_render(drawable);
    }

    // depth of field
    if(frame_asset.settings.dof.enabled)
    {
        auto drawable = m_drawable_dof;
        drawable.descriptors[0].images = {output_img, depth};

        // pass projection matrix (vierkant::Renderer will extract near-/far-clipping planes)
        drawable.matrices.projection = cam->projection_matrix();

        if(!drawable.descriptors[1].buffers.empty())
        {
            frame_asset.settings.dof.clipping = vierkant::clipping_distances(cam->projection_matrix());
            drawable.descriptors[1].buffers.front()->set_data(&frame_asset.settings.dof,
                                                              sizeof(vierkant::dof_settings_t));
        }
        vierkant::semaphore_submit_info_t dof_semaphore_info = {};
        dof_semaphore_info.semaphore = frame_asset.timeline.handle();
        dof_semaphore_info.signal_value = SemaphoreValue::DEFOCUS_BLUR;
        frame_asset.semaphore_value_done = SemaphoreValue::DEFOCUS_BLUR;
        output_img = pingpong_render(drawable, {dof_semaphore_info});
    }

    // draw final color+depth with provided renderer
    m_draw_context.draw_image_fullscreen(renderer, output_img, depth, true);
}

const vierkant::Framebuffer &PBRDeferred::g_buffer() const
{
    size_t last_index =
            (m_g_renderer_pre.num_indices() + m_g_renderer_pre.current_index() - 1) % m_g_renderer_pre.num_indices();
    return m_frame_assets[last_index].g_buffer_post;
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

void vierkant::PBRDeferred::resize_storage(vierkant::PBRDeferred::frame_assets_t &asset,
                                           const glm::uvec2 &resolution)
{
    glm::uvec2 previous_size = {asset.g_buffer_post.extent().width, asset.g_buffer_post.extent().height};
    asset.settings.resolution = glm::max(resolution, glm::uvec2(16));

    VkExtent3D size = {asset.settings.resolution.x, asset.settings.resolution.y, 1};

    VkViewport viewport = {};
    viewport.width = static_cast<float>(size.width);
    viewport.height = static_cast<float>(size.height);
    viewport.maxDepth = 1;

    m_g_renderer_pre.viewport = viewport;
    m_g_renderer_post.viewport = viewport;
    m_light_renderer.viewport = viewport;
    m_sky_renderer.viewport = viewport;
    m_taa_renderer.viewport = viewport;

    // nothing to do
    if(asset.g_buffer_post && asset.g_buffer_post.color_attachment()->extent() == size){ return; }

    m_logger->debug("resizing storage: {} x {} -> {} x {}", previous_size.x, previous_size.y, resolution.x,
                    resolution.y);
    asset.recycle_commands = false;
    vierkant::RenderPassPtr lighting_renderpass, sky_renderpass, post_fx_renderpass;

    // G-buffer (pre and post occlusion-culling)
    asset.g_buffer_pre = create_g_buffer(m_device, size);

    auto renderpass_no_clear_depth =
            vierkant::Framebuffer::create_renderpass(m_device, asset.g_buffer_pre.attachments(), false, false);
    asset.g_buffer_post = vierkant::Framebuffer(m_device, asset.g_buffer_pre.attachments(), renderpass_no_clear_depth);
    asset.g_buffer_post.clear_color = {{0.f, 0.f, 0.f, 0.f}};

    // init lighting framebuffer
    vierkant::Framebuffer::AttachmentMap lighting_attachments, sky_attachments;
    vierkant::Image::Format img_attachment_16f = {};
    img_attachment_16f.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_attachment_16f.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    img_attachment_16f.extent = size;
    lighting_attachments[vierkant::Framebuffer::AttachmentType::Color] = {
            vierkant::Image::create(m_device, img_attachment_16f)};

    sky_attachments = lighting_attachments;

    // use depth from g_buffer
    sky_attachments[vierkant::Framebuffer::AttachmentType::DepthStencil] = {
            asset.g_buffer_post.depth_attachment()};

    lighting_renderpass = vierkant::Framebuffer::create_renderpass(m_device, lighting_attachments, true, false);
    sky_renderpass = vierkant::Framebuffer::create_renderpass(m_device, sky_attachments, false, false);
    asset.lighting_buffer = vierkant::Framebuffer(m_device, lighting_attachments, lighting_renderpass);

    asset.sky_buffer = vierkant::Framebuffer(m_device, sky_attachments, sky_renderpass);

    vierkant::Framebuffer::create_info_t taa_framebuffer_info = {};
    taa_framebuffer_info.color_attachment_format = img_attachment_16f;
    taa_framebuffer_info.size = size;
    asset.taa_buffer = vierkant::Framebuffer(m_device, taa_framebuffer_info);

    vierkant::Framebuffer::create_info_t post_fx_buffer_info = {};
    post_fx_buffer_info.size = size;
    post_fx_buffer_info.color_attachment_format.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    post_fx_buffer_info.color_attachment_format.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // create renderer for g-buffer-pass
    vierkant::Renderer::create_info_t post_render_info = {};
    post_render_info.num_frames_in_flight = m_frame_assets.size();
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport = viewport;
    post_render_info.pipeline_cache = m_pipeline_cache;

    // create post_fx ping pong buffers and renderers
    for(auto &post_fx_ping_pong: asset.post_fx_ping_pongs)
    {
        post_fx_ping_pong.framebuffer = vierkant::Framebuffer(m_device, post_fx_buffer_info, post_fx_renderpass);
        post_fx_ping_pong.framebuffer.clear_color = {{0.f, 0.f, 0.f, 0.f}};
        post_fx_ping_pong.renderer = vierkant::Renderer(m_device, post_render_info);
    }

    // create bloom
    Bloom::create_info_t bloom_info = {};
    bloom_info.size = size;
    bloom_info.size.width = std::max(1U, bloom_info.size.width / 2);
    bloom_info.size.height = std::max(1U, bloom_info.size.height / 2);
    bloom_info.num_blur_iterations = 3;
    asset.bloom = Bloom::create(m_device, bloom_info);
}

void PBRDeferred::create_depth_pyramid(frame_assets_t &frame_asset)
{
    auto extent_pyramid_lvl0 = frame_asset.depth_map->extent();
    extent_pyramid_lvl0.width = crocore::next_pow_2(1 + extent_pyramid_lvl0.width / 2);
    extent_pyramid_lvl0.height = crocore::next_pow_2(1 + extent_pyramid_lvl0.height / 2);

    // create/resize depth pyramid
    if(!frame_asset.depth_pyramid || frame_asset.depth_pyramid->extent() != extent_pyramid_lvl0)
    {
        vierkant::Image::Format depth_pyramid_fmt = {};
        depth_pyramid_fmt.extent = extent_pyramid_lvl0;
        depth_pyramid_fmt.format = VK_FORMAT_R32_SFLOAT;
        depth_pyramid_fmt.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        depth_pyramid_fmt.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depth_pyramid_fmt.use_mipmap = true;
        depth_pyramid_fmt.autogenerate_mipmaps = false;
        depth_pyramid_fmt.reduction_mode = VK_SAMPLER_REDUCTION_MODE_MIN;
        depth_pyramid_fmt.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
        frame_asset.depth_pyramid = vierkant::Image::create(m_device, depth_pyramid_fmt);
    }

    std::vector<VkImageView> pyramid_views = {frame_asset.depth_map->image_view()};
    std::vector<vierkant::ImagePtr> pyramid_images = {frame_asset.depth_map};
    pyramid_images.resize(1 + frame_asset.depth_pyramid->num_mip_levels(), frame_asset.depth_pyramid);

    pyramid_views.insert(pyramid_views.end(), frame_asset.depth_pyramid->mip_image_views().begin(),
                         frame_asset.depth_pyramid->mip_image_views().end());

    vierkant::Compute::computable_t computable = m_depth_pyramid_computable;

    descriptor_t input_sampler_desc = {};
    input_sampler_desc.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    input_sampler_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    computable.descriptors[0] = input_sampler_desc;

    descriptor_t output_image_desc = {};
    output_image_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    output_image_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    computable.descriptors[1] = output_image_desc;

    descriptor_t ubo_desc = {};
    ubo_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    computable.descriptors[2] = ubo_desc;

    vierkant::Compute::create_info_t compute_info = {};
    compute_info.pipeline_cache = m_pipeline_cache;
    compute_info.command_pool = m_command_pool;

    for(uint32_t i = frame_asset.depth_pyramid_computes.size(); i < frame_asset.depth_pyramid->num_mip_levels(); ++i)
    {
        frame_asset.depth_pyramid_computes.emplace_back(m_device, compute_info);
    }

    // transition all mips to general layout for writing
    frame_asset.depth_pyramid->transition_layout(VK_IMAGE_LAYOUT_GENERAL,
                                                 frame_asset.depth_pyramid_cmd_buffer.handle());

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = frame_asset.depth_pyramid->image();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    frame_asset.depth_pyramid_cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
    frame_asset.depth_pyramid_cmd_buffer.begin();

    for(uint32_t lvl = 1; lvl < pyramid_views.size(); ++lvl)
    {
        auto width = std::max(1u, extent_pyramid_lvl0.width >> (lvl - 1));
        auto height = std::max(1u, extent_pyramid_lvl0.height >> (lvl - 1));

        computable.extent = {vierkant::group_count(width, m_depth_pyramid_local_size.x),
                             vierkant::group_count(height, m_depth_pyramid_local_size.y), 1};

        computable.descriptors[0].images = {pyramid_images[lvl - 1]};
        computable.descriptors[0].image_views = {pyramid_views[lvl - 1]};

        computable.descriptors[1].images = {pyramid_images[lvl]};
        computable.descriptors[1].image_views = {pyramid_views[lvl]};

        glm::vec2 image_size = {width, height};
        auto ubo_buffer = vierkant::Buffer::create(m_device, &image_size, sizeof(glm::vec2),
                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        computable.descriptors[2].buffers = {ubo_buffer};

        // dispatch compute shader
        frame_asset.depth_pyramid_computes[lvl - 1].dispatch({computable},
                                                             frame_asset.depth_pyramid_cmd_buffer.handle());

        barrier.subresourceRange.baseMipLevel = lvl - 1;
        vkCmdPipelineBarrier(frame_asset.depth_pyramid_cmd_buffer.handle(),
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
    }

    vierkant::semaphore_submit_info_t pyramid_semaphore_submit_info = {};
    pyramid_semaphore_submit_info.semaphore = frame_asset.timeline.handle();
    pyramid_semaphore_submit_info.wait_value = SemaphoreValue::G_BUFFER_LAST_VISIBLE;
    pyramid_semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    pyramid_semaphore_submit_info.signal_value = SemaphoreValue::DEPTH_PYRAMID;
    frame_asset.depth_pyramid_cmd_buffer.submit(m_queue, false, VK_NULL_HANDLE, {pyramid_semaphore_submit_info});
}

void PBRDeferred::resize_indirect_draw_buffers(uint32_t num_draws,
                                               Renderer::indirect_draw_params_t &params,
                                               VkCommandBuffer clear_cmd_handle)
{
    const size_t num_bytes = num_draws * sizeof(Renderer::indexed_indirect_command_t);

    if(!params.draws_in || params.draws_in->num_bytes() < num_bytes)
    {
        params.draws_in = vierkant::Buffer::create(m_device, nullptr, num_bytes,
                                                   VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VMA_MEMORY_USAGE_GPU_ONLY);
    }

    if(!params.draws_out || params.draws_out->num_bytes() < num_bytes)
    {
        params.draws_out = vierkant::Buffer::create(m_device, nullptr, num_bytes,
                                                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                    VMA_MEMORY_USAGE_GPU_ONLY);
    }

    if(!params.draws_counts_out)
    {
        constexpr uint32_t max_batches = 4096;
        params.draws_counts_out = vierkant::Buffer::create(m_device, nullptr,
                                                           max_batches * sizeof(uint32_t),
                                                           VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                           VMA_MEMORY_USAGE_GPU_ONLY);

        if(clear_cmd_handle)
        {
            vkCmdFillBuffer(clear_cmd_handle, params.draws_counts_out->handle(), 0, VK_WHOLE_SIZE, 0);

            VkBufferMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
            barrier.buffer = params.draws_counts_out->handle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

            VkDependencyInfo dependency_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency_info.bufferMemoryBarrierCount = 1;
            dependency_info.pBufferMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(clear_cmd_handle, &dependency_info);
        }
    }
    params.num_draws = num_draws;
}

void PBRDeferred::cull_draw_commands(frame_assets_t &frame_asset,
                                     const vierkant::CameraPtr &cam,
                                     const vierkant::ImagePtr &depth_pyramid,
                                     const vierkant::BufferPtr &draws_in,
                                     uint32_t num_draws,
                                     vierkant::BufferPtr &draws_out,
                                     vierkant::BufferPtr &draws_counts_out,
                                     vierkant::BufferPtr &draws_out_post,
                                     vierkant::BufferPtr &draws_counts_out_post)
{
    if(!draws_in || !draws_in->num_bytes() || !depth_pyramid)
    {
        frame_asset.timeline.wait(SemaphoreValue::DEPTH_PYRAMID);
        frame_asset.timeline.signal(SemaphoreValue::CULLING);
        return;
    }

    draw_cull_data_t draw_cull_data = {};
    draw_cull_data.num_draws = num_draws;
    draw_cull_data.pyramid_size = {depth_pyramid->width(), depth_pyramid->height()};
    draw_cull_data.occlusion_enabled = frame_asset.settings.occlusion_culling;
    draw_cull_data.distance_cull = false;
    draw_cull_data.culling_enabled = frame_asset.settings.frustum_culling;

    auto projection = cam->projection_matrix();
    draw_cull_data.P00 = projection[0][0];
    draw_cull_data.P11 = projection[1][1];
    draw_cull_data.znear = cam->near();
    draw_cull_data.zfar = cam->far();
    draw_cull_data.view = cam->view_matrix();

    glm::mat4 projectionT = transpose(projection);
    glm::vec4 frustumX = projectionT[3] + projectionT[0]; // x + w < 0
    frustumX /= glm::length(frustumX.xyz());
    glm::vec4 frustumY = projectionT[3] + projectionT[1]; // y + w < 0
    frustumY /= glm::length(frustumY.xyz());

    draw_cull_data.frustum = {frustumX.x, frustumX.z, frustumY.y, frustumY.z};

    draw_cull_result_t result = {};

    if(!frame_asset.cull_ubo)
    {
        frame_asset.cull_ubo = vierkant::Buffer::create(m_device, &draw_cull_data, sizeof(draw_cull_data),
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                        VMA_MEMORY_USAGE_CPU_TO_GPU);

        frame_asset.cull_result_buffer = vierkant::Buffer::create(m_device, &result, sizeof(draw_cull_result_t),
                                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                  VMA_MEMORY_USAGE_GPU_ONLY);

        frame_asset.cull_result_buffer_host = vierkant::Buffer::create(m_device, &result, sizeof(draw_cull_result_t),
                                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                       VMA_MEMORY_USAGE_CPU_ONLY);
    }
    else
    {
        frame_asset.cull_ubo->set_data(&draw_cull_data, sizeof(draw_cull_data));

        // read results from host-buffer
        auto &result_buf = *reinterpret_cast<draw_cull_result_t *>(frame_asset.cull_result_buffer_host->map());
        result = result_buf;

        m_logger->trace("num_draws: {} -- frustum-culled: {} -- occlusion-culled: {} -- num_triangles: {}",
                        result.draw_count, result.num_frustum_culled, result.num_occlusion_culled,
                        result.num_triangles);
    }

    vierkant::Compute::computable_t computable = m_cull_computable;

    descriptor_t depth_pyramid_desc = {};
    depth_pyramid_desc.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    depth_pyramid_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    depth_pyramid_desc.images = {depth_pyramid};
    computable.descriptors[0] = depth_pyramid_desc;

    descriptor_t draw_cull_data_desc = {};
    draw_cull_data_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    draw_cull_data_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    draw_cull_data_desc.buffers = {frame_asset.cull_ubo};
    computable.descriptors[1] = draw_cull_data_desc;

    descriptor_t in_buffer_desc = {};
    in_buffer_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    in_buffer_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    in_buffer_desc.buffers = {draws_in};
    computable.descriptors[2] = in_buffer_desc;

    descriptor_t out_buffer_desc = {};
    out_buffer_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out_buffer_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    out_buffer_desc.buffers = {draws_out};
    computable.descriptors[3] = out_buffer_desc;

    descriptor_t out_counts_buffer_desc = {};
    out_counts_buffer_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out_counts_buffer_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    out_counts_buffer_desc.buffers = {draws_counts_out};
    computable.descriptors[4] = out_counts_buffer_desc;

    descriptor_t out_buffer_post_desc = {};
    out_buffer_post_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out_buffer_post_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    out_buffer_post_desc.buffers = {draws_out_post};
    computable.descriptors[5] = out_buffer_post_desc;

    descriptor_t out_counts_buffer_post_desc = {};
    out_counts_buffer_post_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out_counts_buffer_post_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    out_counts_buffer_post_desc.buffers = {draws_counts_out_post};
    computable.descriptors[6] = out_counts_buffer_post_desc;

    descriptor_t out_result_desc = {};
    out_result_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out_result_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    out_result_desc.buffers = {frame_asset.cull_result_buffer};
    computable.descriptors[7] = out_result_desc;

    // create / start a new command-buffer
    frame_asset.cull_cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
    frame_asset.cull_cmd_buffer.begin();

    // clear gpu-result-buffer with zeros
    vkCmdFillBuffer(frame_asset.cull_cmd_buffer.handle(), frame_asset.cull_result_buffer->handle(), 0, VK_WHOLE_SIZE,
                    0);

    // clear count-buffers with zeros
    vkCmdFillBuffer(frame_asset.cull_cmd_buffer.handle(), draws_counts_out->handle(), 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(frame_asset.cull_cmd_buffer.handle(), draws_counts_out_post->handle(), 0, VK_WHOLE_SIZE, 0);

    if(!frame_asset.cull_compute)
    {
        vierkant::Compute::create_info_t compute_info = {};
        compute_info.pipeline_cache = m_pipeline_cache;
        compute_info.command_pool = m_command_pool;
        frame_asset.cull_compute = vierkant::Compute(m_device, compute_info);
    }

    computable.extent = {vierkant::group_count(num_draws, m_cull_compute_local_size.x), 1, 1};

    VkBufferMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    barrier.buffer = draws_out->handle();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    VkDependencyInfo dependency_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency_info.bufferMemoryBarrierCount = 1;
    dependency_info.pBufferMemoryBarriers = &barrier;

    // barrier before writing to indirect-draw-buffer
    vkCmdPipelineBarrier2(frame_asset.cull_cmd_buffer.handle(), &dependency_info);

    // dispatch cull-compute
    frame_asset.cull_compute.dispatch({computable}, frame_asset.cull_cmd_buffer.handle());

    // swap access
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

    // memory barrier for draw-indirect buffer
    vkCmdPipelineBarrier2(frame_asset.cull_cmd_buffer.handle(), &dependency_info);

    // memory barrier before copying cull-result buffer
    barrier.buffer = frame_asset.cull_result_buffer->handle();
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    vkCmdPipelineBarrier2(frame_asset.cull_cmd_buffer.handle(), &dependency_info);

    // copy result into host-visible buffer
    frame_asset.cull_result_buffer->copy_to(frame_asset.cull_result_buffer_host, frame_asset.cull_cmd_buffer.handle());

    vierkant::semaphore_submit_info_t culling_semaphore_submit_info = {};
    culling_semaphore_submit_info.semaphore = frame_asset.timeline.handle();
    culling_semaphore_submit_info.wait_value = SemaphoreValue::DEPTH_PYRAMID;
    culling_semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    culling_semaphore_submit_info.signal_value = SemaphoreValue::CULLING;
    frame_asset.cull_cmd_buffer.submit(m_queue, false, VK_NULL_HANDLE, {culling_semaphore_submit_info});
}

bool operator==(const PBRDeferred::settings_t &lhs, const PBRDeferred::settings_t &rhs)
{
    if(lhs.resolution != rhs.resolution){ return false; }
    if(lhs.disable_material != rhs.disable_material){ return false; }
    if(lhs.frustum_culling != rhs.frustum_culling){ return false; }
    if(lhs.occlusion_culling != rhs.occlusion_culling){ return false; }
    if(lhs.tesselation != rhs.tesselation){ return false; }
    if(lhs.wireframe != rhs.wireframe){ return false; }
    if(lhs.draw_skybox != rhs.draw_skybox){ return false; }
    if(lhs.use_fxaa != rhs.use_fxaa){ return false; }
    if(lhs.use_taa != rhs.use_taa){ return false; }
    if(lhs.tonemap != rhs.tonemap){ return false; }
    if(lhs.bloom != rhs.bloom){ return false; }
    if(lhs.motionblur != rhs.motionblur){ return false; }
    if(lhs.motionblur_gain != rhs.motionblur_gain){ return false; }
    if(lhs.gamma != rhs.gamma){ return false; }
    if(lhs.exposure != rhs.exposure){ return false; }
    if(lhs.dof != rhs.dof){ return false; }
    return true;
}

}// namespace vierkant