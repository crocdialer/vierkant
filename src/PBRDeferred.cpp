#include <crocore/gaussian.hpp>

#include <vierkant/PBRDeferred.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/cubemap_utils.hpp>
#include <vierkant/culling.hpp>
#include <vierkant/punctual_light.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/staging_copy.hpp>

namespace vierkant
{

const char *PBRDeferred::to_string(PBRDeferred::SemaphoreValue v)
{
    switch(v)
    {
        case INVALID: return "INVALID";
        case PRE_RENDER: return "PRE_RENDER";
        case G_BUFFER_LAST_VISIBLE: return "G_BUFFER_LAST_VISIBLE";
        case DEPTH_PYRAMID: return "DEPTH_PYRAMID";
        case CULLING: return "CULLING";
        case G_BUFFER_ALL: return "G_BUFFER_ALL";
        case AMBIENT_OCCLUSION: return "AMBIENT_OCCLUSION";
        case LIGHTING: return "LIGHTING";
        case TAA: return "TAA";
        case BLOOM: return "BLOOM";
        case TONEMAP: return "TONEMAP";
        case FXAA: return "FXAA";
        case DEFOCUS_BLUR: return "DEFOCUS_BLUR";
        case MAX_VALUE: break;
    }
    assert(false);
    return "";
}

PBRDeferred::PBRDeferred(const DevicePtr &device, const create_info_t &create_info)
    : m_device(device), m_hdr_format(create_info.hdr_format)
{
    m_logger = create_info.logger_name.empty() ? spdlog::default_logger() : spdlog::get(create_info.logger_name);
    m_queue = create_info.queue ? create_info.queue : device->queue();

    m_command_pool = vierkant::create_command_pool(m_device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                           VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // create a DescriptorPool
    if(!create_info.descriptor_pool)
    {

        vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8192},
                                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256},
                                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256}};
        m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 1024);
    }
    else { m_descriptor_pool = create_info.descriptor_pool; }

    m_pipeline_cache =
            create_info.pipeline_cache ? create_info.pipeline_cache : vierkant::PipelineCache::create(device);

    m_ray_builder = vierkant::RayBuilder(device, m_queue);

    m_frame_contexts.resize(create_info.num_frames_in_flight);

    for(auto &frame_context: m_frame_contexts)
    {
        frame_context.timeline = vierkant::Semaphore(m_device);

        resize_storage(frame_context, create_info.settings.resolution, create_info.settings.output_resolution);

        frame_context.g_buffer_camera_ubo = vierkant::Buffer::create(
                m_device, nullptr, sizeof(glm::vec2), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        frame_context.lighting_param_ubo =
                vierkant::Buffer::create(device, nullptr, sizeof(environment_lighting_ubo_t),
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        frame_context.lights_ubo =
                vierkant::Buffer::create(device, nullptr, sizeof(vierkant::lightsource_ubo_t),
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        vierkant::Buffer::create_info_t composition_buffer_info = {};
        composition_buffer_info.device = m_device;
        composition_buffer_info.num_bytes = sizeof(composition_ubo_t);
        composition_buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        composition_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        composition_buffer_info.name = "composition_ubo";
        frame_context.composition_ubo = vierkant::Buffer::create(composition_buffer_info);

        vierkant::Buffer::create_info_t anim_buffer_info = {};
        anim_buffer_info.device = m_device;
        anim_buffer_info.num_bytes = 1U << 20U;
        anim_buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        anim_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
        anim_buffer_info.name = "bone_buffer";
        frame_context.bone_buffer = vierkant::Buffer::create(anim_buffer_info);
        anim_buffer_info.name = "morph_param_buffer";
        frame_context.morph_param_buffer = vierkant::Buffer::create(anim_buffer_info);

        frame_context.query_pool =
                vierkant::create_query_pool(m_device, SemaphoreValue::MAX_VALUE * 2, VK_QUERY_TYPE_TIMESTAMP);
        frame_context.gpu_cull_context = vierkant::create_gpu_cull_context(device, m_pipeline_cache);

        // create staging-buffers
        vierkant::Buffer::create_info_t staging_buffer_info = {};
        staging_buffer_info.device = m_device;
        staging_buffer_info.num_bytes = 1U << 20U;
        staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_ONLY;
        staging_buffer_info.name = "staging_main";
        frame_context.staging_main = vierkant::Buffer::create(staging_buffer_info);
        staging_buffer_info.name = "staging_anim";
        frame_context.staging_anim = vierkant::Buffer::create(staging_buffer_info);
        staging_buffer_info.name = "staging_post_fx";
        frame_context.staging_post_fx = vierkant::Buffer::create(staging_buffer_info);

        vierkant::CommandBuffer::create_info_t command_buffer_info = {};
        command_buffer_info.device = m_device;
        command_buffer_info.command_pool = m_command_pool.get();
        command_buffer_info.name = "PBRDeferred::cmd_pre_render";
        frame_context.cmd_pre_render = vierkant::CommandBuffer(command_buffer_info);

        command_buffer_info.name = "PBRDeferred::cmd_post_fx";
        frame_context.cmd_post_fx = vierkant::CommandBuffer(command_buffer_info);

        command_buffer_info.name = "PBRDeferred::cmd_lighting";
        frame_context.cmd_lighting = vierkant::CommandBuffer(command_buffer_info);

        command_buffer_info.name = "PBRDeferred::cmd_clear";
        frame_context.cmd_clear = vierkant::CommandBuffer(command_buffer_info);

        command_buffer_info.name = "PBRDeferred::cmd_copy_object_id";
        frame_context.cmd_copy_object_id = vierkant::CommandBuffer(command_buffer_info);
    }

    // create renderer for g-buffer-pass
    vierkant::Rasterizer::create_info_t render_create_info = {};
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.queue = create_info.queue;
    render_create_info.sample_count = create_info.sample_count;
    render_create_info.viewport.width = static_cast<float>(create_info.settings.resolution.x);
    render_create_info.viewport.height = static_cast<float>(create_info.settings.resolution.y);
    render_create_info.descriptor_pool = m_descriptor_pool;
    render_create_info.pipeline_cache = m_pipeline_cache;
    render_create_info.indirect_draw = true;
    render_create_info.enable_mesh_shader = true;
    m_g_renderer_main = vierkant::Rasterizer(device, render_create_info);
    m_g_renderer_post = vierkant::Rasterizer(device, render_create_info);

    // create renderer for lighting-pass
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    render_create_info.indirect_draw = false;
    auto light_render_create_info = render_create_info;
    light_render_create_info.num_frames_in_flight = 2 * create_info.num_frames_in_flight;
    m_renderer_lighting = vierkant::Rasterizer(device, light_render_create_info);

    // create renderer for post-fx-passes
    constexpr size_t max_num_post_fx_passes = SemaphoreValue::MAX_VALUE - SemaphoreValue::LIGHTING;
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight * max_num_post_fx_passes;
    m_renderer_post_fx = vierkant::Rasterizer(m_device, render_create_info);

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
        vierkant::descriptor_t &desc_ubo = m_drawable_lighting_env.descriptors[0];
        desc_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        vierkant::descriptor_t &desc_texture = m_drawable_lighting_env.descriptors[1];
        desc_texture.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_texture.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        vierkant::descriptor_t &desc_cubes = m_drawable_lighting_env.descriptors[2];
        desc_cubes.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_cubes.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        vierkant::descriptor_t &desc_lights = m_drawable_lighting_env.descriptors[3];
        desc_lights.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_lights.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_drawable_lighting_env.num_vertices = 3;
        m_drawable_lighting_env.pipeline_format = fmt;
        m_drawable_lighting_env.use_own_buffers = true;
    }

    // create drawables for post-fx-pass
    {
        vierkant::drawable_t fullscreen_drawable = {};

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
        desc_taa_ubo.buffers = {vierkant::Buffer::create(
                m_device, nullptr, sizeof(camera_params_t),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY)};

        m_drawable_taa.descriptors[1] = std::move(desc_taa_ubo);

        // fxaa
        m_drawable_fxaa = fullscreen_drawable;
        m_drawable_fxaa.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::fxaa_frag);

        // dof
        m_drawable_dof = fullscreen_drawable;
        m_drawable_dof.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::dof_frag);

        // depth-of-field settings uniform-buffer
        vierkant::descriptor_t &desc_dof_ubo = m_drawable_dof.descriptors[1];
        desc_dof_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_dof_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        desc_dof_ubo.buffers = {vierkant::Buffer::create(
                m_device, nullptr, sizeof(depth_of_field_params_t),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY)};

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

    // solid black/white colors
    uint32_t v = 0xFF000000;
    vierkant::Image::Format fmt;
    fmt.extent = {1, 1, 1};
    fmt.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_util_img_black = vierkant::Image::create(m_device, &v, fmt);
    v = 0xFFFFFFFF;
    m_util_img_white = vierkant::Image::create(m_device, &v, fmt);

    // populate a 2,3 halton sequence
    m_sample_offsets.resize(8);

    for(uint32_t i = 0; i < m_sample_offsets.size(); ++i)
    {
        m_sample_offsets[i] = glm::vec2(crocore::halton(i + 1, 2), crocore::halton(i + 1, 3));
    }
}

PBRDeferred::~PBRDeferred()
{
    for(auto &frame_context: m_frame_contexts)
    {
        frame_context.timeline.wait(frame_context.current_semaphore_value + frame_context.semaphore_value_done);
    }
}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device, const create_info_t &create_info)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device, create_info));
}

void PBRDeferred::update_recycling(const SceneConstPtr &scene, const CameraPtr &cam, frame_context_t &frame_context)
{
    std::unordered_set<vierkant::MeshConstPtr> meshes;
    std::unordered_map<vierkant::id_entry_t, size_t> transform_hashes;

    bool materials_unchanged = true;
    bool objects_unchanged = true;
    frame_context.dirty_drawable_indices.clear();

    size_t scene_hash = 0;
    size_t transform_hash = std::hash<vierkant::transform_t>()(scene->root()->transform);

    auto object_view = scene->registry()->view<vierkant::Object3D *>();
    object_view.each([&scene_hash](const auto &object) {
        vierkant::hash_combine(scene_hash, object);
        vierkant::hash_combine(scene_hash, object->enabled);
    });

    auto mesh_view = scene->registry()->view<vierkant::mesh_component_t>();

    for(const auto &[entity, mesh_component]: mesh_view.each())
    {
        auto object = scene->object_by_id(static_cast<uint32_t>(entity));
        if(!object) { continue; }
        auto obj_global_transform = object->global_transform();

        auto mesh = mesh_component.mesh;
        if(!mesh) { continue; }

        bool transform_update = false;
        meshes.insert(mesh);

        bool animation_update = !mesh->node_animations.empty() && !mesh->root_bone && !mesh->morph_buffer &&
                                object->has_component<animation_component_t>();

        // entry animation transforms
        std::vector<vierkant::transform_t> node_transforms;

        if(animation_update)
        {
            const vierkant::object_component auto &animation_state = object->get_component<animation_component_t>();
            const auto &animation = mesh->node_animations[animation_state.index];
            vierkant::nodes::build_node_matrices_bfs(mesh->root_node, animation,
                                                     static_cast<float>(animation_state.current_time), node_transforms);
        }

        for(uint32_t i = 0; i < mesh->entries.size(); ++i)
        {
            bool entry_enabled = !mesh_component.entry_indices || mesh_component.entry_indices->contains(i);
            vierkant::hash_combine(scene_hash, entry_enabled);
            if(!entry_enabled) { continue; }

            const auto &entry = mesh->entries[i];

            id_entry_t key = {object->id(), i};
            auto it = m_entry_matrix_cache.find(key);

            vierkant::hash_combine(transform_hash, obj_global_transform * entry.transform);
            transform_hashes[key] = transform_hash;

            auto hash_it = frame_context.transform_hashes.find(key);
            transform_update = transform_update ||
                               (hash_it != frame_context.transform_hashes.end() && hash_it->second != transform_hash);

            if((transform_update || animation_update) && frame_context.cull_result.index_map.contains(key))
            {
                // combine mesh- with entry-transform
                uint32_t drawable_index = frame_context.cull_result.index_map.at(key);
                frame_context.dirty_drawable_indices.insert(drawable_index);

                auto &drawable = frame_context.cull_result.drawables[drawable_index];
                drawable.matrices.transform = node_transforms.empty()
                                                      ? object->global_transform() * entry.transform
                                                      : object->global_transform() * node_transforms[entry.node_index];
                drawable.last_matrices =
                        it != m_entry_matrix_cache.end() ? it->second : std::optional<matrix_struct_t>();

                m_entry_matrix_cache[key] = drawable.matrices;
            }
        }
    }

    frame_context.transform_hashes = std::move(transform_hashes);

    if(scene_hash != frame_context.scene_hash)
    {
        objects_unchanged = false;
        frame_context.scene_hash = scene_hash;
        frame_context.dirty_drawable_indices.clear();
    }

    for(const auto &mesh: meshes)
    {
        for(const auto &mat: mesh->materials)
        {
            auto h = std::hash<material_t>()(mat->m);
            if(frame_context.material_hashes[mat] != h) { materials_unchanged = false; }
            frame_context.material_hashes[mat] = h;
        }
    }
    bool need_culling = frame_context.cull_result.camera != cam || meshes != frame_context.cull_result.meshes;
    frame_context.recycle_commands =
            objects_unchanged && materials_unchanged && !need_culling && frame_context.settings == settings;

    frame_context.settings = settings;
}

SceneRenderer::render_result_t PBRDeferred::render_scene(Rasterizer &renderer, const SceneConstPtr &scene,
                                                         const CameraPtr &cam, const std::set<std::string> &tags)
{
    // reference to current frame-assets
    auto &frame_context = m_frame_contexts[m_g_renderer_main.current_index()];
    frame_context.timestamp = std::chrono::steady_clock::now();

    // retrieve last performance-query, start new
    update_timing(frame_context);

    // determine if we can re-use commandbuffers, buffers, etc.
    update_recycling(scene, cam, frame_context);

    if(!frame_context.recycle_commands)
    {
        vierkant::cull_params_t cull_params = {};
        cull_params.scene = scene;
        cull_params.camera = cam;
        cull_params.tags = tags;
        cull_params.check_intersection = false;
        cull_params.world_space = true;
        frame_context.cull_result = vierkant::cull(cull_params);
    }

    // timeline semaphore
    frame_context.timeline.wait(frame_context.current_semaphore_value + frame_context.semaphore_value_done);
    frame_context.current_semaphore_value += frame_context.semaphore_value_done;

    // start label
    vierkant::begin_label(m_queue, {"PBRDeferred::render_scene"});

    resize_storage(frame_context, settings.resolution, settings.output_resolution);

    // apply+update transform history
    update_animation_transforms(frame_context);

    // create g-buffer
    auto &g_buffer = geometry_pass(frame_context.cull_result);
    auto albedo_map = g_buffer.color_attachment(G_BUFFER_ALBEDO);

    // default to color image
    auto out_img = albedo_map;
    auto depth_img = frame_context.g_buffer_main.depth_attachment();

    // lighting-pass
    if(m_conv_lambert && m_conv_ggx)
    {
        auto &light_buffer = lighting_pass(frame_context.cull_result);
        out_img = light_buffer.color_attachment(0);
    }

    // dof, bloom, anti-aliasing
    out_img = post_fx_pass(cam, out_img, depth_img);

    // draw final color+depth with provided renderer
    m_draw_context.draw_image_fullscreen(renderer, out_img, depth_img, true);

    // end debug label
    vierkant::end_label(m_queue);

    SceneRenderer::render_result_t ret = {};
    ret.object_by_index_fn = [scene,
                              &cull_result = frame_context.cull_result](uint32_t object_idx) -> vierkant::id_entry_t {
        if(object_idx < cull_result.drawables.size())
        {
            // picked_idx is an index into an array of drawables
            auto drawable_id = cull_result.drawables[object_idx].id;
            return cull_result.entity_map.at(drawable_id);
        }
        return {};
    };
    ret.num_draws = frame_context.cull_result.drawables.size();
    ret.num_frustum_culled = frame_context.stats.draw_cull_result.num_frustum_culled;
    ret.num_occlusion_culled = frame_context.stats.draw_cull_result.num_occlusion_culled;
    ret.num_contribution_culled = frame_context.stats.draw_cull_result.num_contribution_culled;
    ret.object_ids = frame_context.internal_images.object_ids;

    vierkant::semaphore_submit_info_t semaphore_submit_info = {};
    semaphore_submit_info.semaphore = frame_context.timeline.handle();
    semaphore_submit_info.wait_value = frame_context.current_semaphore_value + frame_context.semaphore_value_done;
    semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    ret.semaphore_infos = {semaphore_submit_info};
    return ret;
}

void PBRDeferred::update_animation_transforms(frame_context_t &frame_context)
{
    // cache/collect bone-matrices
    std::unordered_map<uint32_t, size_t> entity_bone_buffer_offsets;
    std::vector<vierkant::transform_t> all_bone_transforms;

    // cache/collect morph-params
    using morph_buffer_offset_mapt_t = std::unordered_map<id_entry_t, size_t>;
    morph_buffer_offset_mapt_t morph_buffer_offsets;
    std::vector<morph_params_t> all_morph_params;

    size_t last_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                        m_g_renderer_main.num_concurrent_frames();
    auto &last_frame_context = m_frame_contexts[last_index];

    auto view = frame_context.cull_result.scene->registry()
                        ->view<vierkant::mesh_component_t, vierkant::animation_component_t>();

    for(const auto &[entity, mesh_component, animation_state]: view.each())
    {
        auto object_id = static_cast<uint32_t>(entity);
        const auto &mesh = mesh_component.mesh;
        const auto &animation = mesh->node_animations[animation_state.index];

        if(mesh->root_bone)
        {
            std::vector<vierkant::transform_t> bone_transforms;
            vierkant::nodes::build_node_matrices_bfs(mesh->root_bone, animation, animation_state.current_time,
                                                     bone_transforms);

            // min alignment for storage-buffers
            auto min_alignment = m_device->properties().core.limits.minStorageBufferOffsetAlignment;
            size_t num_bytes = bone_transforms.size() * sizeof(vierkant::transform_t);
            if(num_bytes % min_alignment) { bone_transforms.push_back({}); }

            // keep track of offset
            entity_bone_buffer_offsets[object_id] = all_bone_transforms.size() * sizeof(vierkant::transform_t);
            all_bone_transforms.insert(all_bone_transforms.end(), bone_transforms.begin(), bone_transforms.end());
        }
        else if(mesh->morph_buffer)
        {
            // morph-target weights
            std::vector<std::vector<float>> node_morph_weights;
            vierkant::nodes::build_morph_weights_bfs(mesh->root_node, animation, animation_state.current_time,
                                                     node_morph_weights);

            for(uint32_t i = 0; i < mesh->entries.size(); ++i)
            {
                const auto &entry = mesh->entries[i];
                id_entry_t key = {object_id, i};
                const auto &weights = node_morph_weights[entry.node_index];

                morph_params_t p;
                p.base_vertex = entry.morph_vertex_offset;
                p.vertex_count = entry.num_vertices;
                p.morph_count = weights.size();

                assert(p.morph_count * sizeof(float) <= sizeof(p.weights));
                memcpy(p.weights, weights.data(), weights.size() * sizeof(float));

                // keep track of offset
                morph_buffer_offsets[key] = all_morph_params.size() * sizeof(morph_params_t);
                all_morph_params.push_back(p);
            }
        }
    }

    frame_context.cmd_pre_render.begin(0);

    // barriers
    VkBuffer buffers[] = {frame_context.bone_buffer->handle(), frame_context.morph_param_buffer->handle()};
    vierkant::barrier(frame_context.cmd_pre_render.handle(), buffers, 2, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    vierkant::staging_copy_context_t staging_context = {};
    staging_context.staging_buffer = frame_context.staging_anim;
    staging_context.command_buffer = frame_context.cmd_pre_render.handle();

    vierkant::staging_copy_info_t copy_bones = {};
    copy_bones.num_bytes = all_bone_transforms.size() * sizeof(vierkant::transform_t);
    copy_bones.data = all_bone_transforms.data();
    copy_bones.dst_buffer = frame_context.bone_buffer;
    copy_bones.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
    copy_bones.dst_stage = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;

    vierkant::staging_copy_info_t copy_morphs = {};
    copy_morphs.num_bytes = all_morph_params.size() * sizeof(morph_params_t);
    copy_morphs.data = all_morph_params.data();
    copy_morphs.dst_buffer = frame_context.morph_param_buffer;
    copy_morphs.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
    copy_morphs.dst_stage = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;

    vierkant::staging_copy(staging_context, {copy_bones, copy_morphs});

    vierkant::semaphore_submit_info_t semaphore_info = {};
    semaphore_info.semaphore = frame_context.timeline.handle();
    semaphore_info.signal_value = frame_context.current_semaphore_value + SemaphoreValue::PRE_RENDER;
    frame_context.cmd_pre_render.submit(m_queue, false, VK_NULL_HANDLE, {semaphore_info});

    if(!frame_context.recycle_commands)
    {
        // insert previous matrices from cache, if any
        for(auto &drawable: frame_context.cull_result.drawables)
        {
            auto [entity, sub_entry] = frame_context.cull_result.entity_map[drawable.id];

            // search previous matrices
            id_entry_t key = {static_cast<uint32_t>(entity), drawable.entry_index};
            auto it = m_entry_matrix_cache.find(key);
            if(it != m_entry_matrix_cache.end()) { drawable.last_matrices = it->second; }

            // descriptors for bone buffers, if necessary
            if(drawable.mesh && drawable.mesh->root_bone)
            {
                uint32_t buffer_offset = entity_bone_buffer_offsets[entity];

                vierkant::descriptor_t desc_bones = {};
                desc_bones.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_bones.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
                desc_bones.buffers = {frame_context.bone_buffer};
                desc_bones.buffer_offsets = {buffer_offset};
                drawable.descriptors[Rasterizer::BINDING_BONES] = desc_bones;

                if(last_frame_context.bone_buffer &&
                   last_frame_context.bone_buffer->num_bytes() == frame_context.bone_buffer->num_bytes())
                {
                    desc_bones.buffers = {last_frame_context.bone_buffer};
                }
                drawable.descriptors[Rasterizer::BINDING_PREVIOUS_BONES] = desc_bones;
            }

            if(drawable.mesh && drawable.mesh->morph_buffer)
            {
                //! morph_params_t contains information to access a morph-target buffer
                uint32_t buffer_offset = morph_buffer_offsets[key];

                // use combined buffer
                vierkant::descriptor_t desc_morph_params = {};
                desc_morph_params.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_morph_params.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
                desc_morph_params.buffers = {frame_context.morph_param_buffer};
                desc_morph_params.buffer_offsets = {buffer_offset};
                drawable.descriptors[Rasterizer::BINDING_MORPH_PARAMS] = desc_morph_params;

                if(last_frame_context.morph_param_buffer &&
                   last_frame_context.morph_param_buffer->num_bytes() == frame_context.morph_param_buffer->num_bytes())
                {
                    desc_morph_params.buffers = {last_frame_context.morph_param_buffer};
                }
                drawable.descriptors[Rasterizer::BINDING_PREVIOUS_MORPH_PARAMS] = desc_morph_params;
            }
        }
    }
}

vierkant::Framebuffer &PBRDeferred::geometry_pass(cull_result_t &cull_result)
{
    auto &frame_context = m_frame_contexts[m_g_renderer_main.current_index()];

    size_t last_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                        m_g_renderer_main.num_concurrent_frames();
    auto &last_frame_context = m_frame_contexts[last_index];

    // jitter state
    constexpr float halton_multiplier = 1.f;
    const glm::vec2 pixel_step =
            1.f / glm::vec2(frame_context.g_buffer_post.extent().width, frame_context.g_buffer_post.extent().height);

    glm::vec2 jitter_offset = halton_multiplier * pixel_step * (m_sample_offsets[m_sample_index] - glm::vec2(.5f));
    jitter_offset = settings.use_taa ? jitter_offset : glm::vec2(0);
    m_sample_index = (m_sample_index + 1) % m_sample_offsets.size();

    // update camera/jitter ubo
    frame_context.camera_params = {};
    frame_context.camera_params.view = vierkant::mat4_cast(cull_result.camera->view_transform());
    frame_context.camera_params.projection = cull_result.camera->projection_matrix();
    frame_context.camera_params.sample_offset = jitter_offset;
    frame_context.camera_params.near = cull_result.camera->near();
    frame_context.camera_params.far = cull_result.camera->far();

    glm::mat4 projectionT = transpose(cull_result.camera->projection_matrix());
    glm::vec4 frustumX = projectionT[3] + projectionT[0];// x + w < 0
    frustumX /= glm::length(frustumX.xyz());
    glm::vec4 frustumY = projectionT[3] + projectionT[1];// y + w < 0
    frustumY /= glm::length(frustumY.xyz());
    frame_context.camera_params.frustum = {frustumX.x, frustumX.z, frustumY.y, frustumY.z};

    camera_params_t cameras[2] = {frame_context.camera_params, last_frame_context.camera_params};
    frame_context.g_buffer_camera_ubo->set_data(&cameras, sizeof(cameras));

    // decide on indirect rendering-path
    bool use_gpu_culling = frame_context.settings.indirect_draw &&
                           (frame_context.settings.frustum_culling || frame_context.settings.occlusion_culling ||
                            frame_context.settings.enable_lod);

    if(!frame_context.recycle_commands)
    {
        vierkant::descriptor_t camera_desc = {};
        camera_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_desc.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
        camera_desc.buffers = {frame_context.g_buffer_camera_ubo};

        // draw all geometry
        for(auto &drawable: cull_result.drawables)
        {
            uint32_t shader_flags = PROP_DEFAULT;

            // check if vertex-skinning is required
            if(drawable.mesh->root_bone) { shader_flags |= PROP_SKIN; }

            // check if tangents are available
            if(drawable.mesh->vertex_attribs.count(Mesh::ATTRIB_TANGENT)) { shader_flags |= PROP_TANGENT_SPACE; }

            // attribute/binding descriptions obsolete here
            drawable.pipeline_format.attribute_descriptions.clear();
            drawable.pipeline_format.binding_descriptions.clear();

            const bool use_meshlet_pipeline = drawable.mesh->meshlets && frame_context.settings.use_meshlet_pipeline &&
                                              !drawable.mesh->morph_buffer && !drawable.mesh->root_bone;

            if(use_meshlet_pipeline)
            {
                shader_flags |= PROP_MESHLETS;
                camera_desc.stage_flags |= VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;

                auto &mesh_shader_props = m_device->properties().mesh_shader;
                vierkant::pipeline_specialization pipeline_specialization;
                pipeline_specialization.set(0, mesh_shader_props.maxPreferredTaskWorkGroupInvocations);
                pipeline_specialization.set(1, mesh_shader_props.maxPreferredMeshWorkGroupInvocations);
                drawable.pipeline_format.specialization = std::move(pipeline_specialization);
            }

            // check if morph-targets are available
            if(drawable.mesh->morph_buffer) { shader_flags |= PROP_MORPH_TARGET; }

            // select shader-stages from cache
            auto stage_it = m_g_buffer_shader_stages.find(shader_flags);

            // fallback to default if not found
            if(stage_it != m_g_buffer_shader_stages.end())
            {
                drawable.pipeline_format.shader_stages = stage_it->second;
            }
            else { drawable.pipeline_format.shader_stages = m_g_buffer_shader_stages[PROP_DEFAULT]; }

            // set attachment count
            drawable.pipeline_format.attachment_count = G_BUFFER_SIZE;

            // disable blending
            drawable.pipeline_format.blend_state.blendEnable = false;
            drawable.pipeline_format.depth_test = true;
            drawable.pipeline_format.depth_write = true;

            // optional wireframe rendering
            drawable.pipeline_format.polygon_mode =
                    frame_context.settings.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

            // add descriptor for a jitter-offset
            drawable.descriptors[Rasterizer::BINDING_JITTER_OFFSET] = camera_desc;
        }
        // stage drawables
        m_g_renderer_main.stage_drawables(cull_result.drawables);
        if(use_gpu_culling) { m_g_renderer_post.stage_drawables(cull_result.drawables); }
    }

    // apply current settings for both renderers
    m_g_renderer_main.disable_material = m_g_renderer_post.disable_material = frame_context.settings.disable_material;
    m_g_renderer_main.debug_draw_ids = m_g_renderer_post.debug_draw_ids = frame_context.settings.debug_draw_ids;
    m_g_renderer_main.indirect_draw = m_g_renderer_post.indirect_draw = frame_context.settings.indirect_draw;
    m_g_renderer_main.use_mesh_shader = m_g_renderer_post.use_mesh_shader = frame_context.settings.use_meshlet_pipeline;

    // draw last visible objects
    m_g_renderer_main.draw_indirect_delegate = [this, &frame_context,
                                                use_gpu_culling](Rasterizer::indirect_draw_bundle_t &params) {
        frame_context.cmd_clear.begin(0);

        auto src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        auto src_access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        auto dst_stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
        auto dst_access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

        resize_indirect_draw_buffers(params.num_draws, frame_context.indirect_draw_params_main);
        frame_context.indirect_draw_params_main.draws_out = params.draws_out;
        frame_context.indirect_draw_params_main.mesh_draws = params.mesh_draws;
        frame_context.indirect_draw_params_main.mesh_entries = params.mesh_entries;

        vierkant::staging_copy_context_t staging_context = {};
        staging_context.staging_buffer = frame_context.staging_main;
        staging_context.command_buffer = frame_context.cmd_clear.handle();

        if(params.num_draws && !frame_context.recycle_commands)
        {
            params.draws_in->copy_to(frame_context.indirect_draw_params_main.draws_out,
                                     frame_context.cmd_clear.handle());
            frame_context.indirect_draw_params_main.draws_out->barrier(frame_context.cmd_clear.handle(), src_stage,
                                                                       src_access, dst_stage, dst_access);

            if(use_gpu_culling)
            {
                frame_context.indirect_draw_params_main.draws_in->barrier(frame_context.cmd_clear.handle(), dst_stage,
                                                                          dst_access, src_stage, src_access);
                params.draws_in->copy_to(frame_context.indirect_draw_params_main.draws_in,
                                         frame_context.cmd_clear.handle());
                frame_context.indirect_draw_params_main.draws_in->barrier(frame_context.cmd_clear.handle(), src_stage,
                                                                          src_access, dst_stage, dst_access);
            }

            if(use_gpu_culling && !params.draws_counts_out)
            {
                params.draws_counts_out = frame_context.indirect_draw_params_main.draws_counts_out;
                vkCmdFillBuffer(frame_context.cmd_clear.handle(),
                                frame_context.indirect_draw_params_main.draws_counts_out->handle(), 0, VK_WHOLE_SIZE,
                                0);
                frame_context.indirect_draw_params_main.draws_counts_out->barrier(
                        frame_context.cmd_clear.handle(), src_stage, src_access, dst_stage, dst_access);
            }
        }
        else if(params.num_draws && !frame_context.dirty_drawable_indices.empty())
        {
            VkBuffer buffers[] = {params.mesh_draws->handle(),
                                  frame_context.indirect_draw_params_post.mesh_draws->handle()};
            vierkant::barrier(frame_context.cmd_clear.handle(), buffers, 2, src_stage, src_access, src_stage,
                              src_access);
            constexpr size_t stride = sizeof(Rasterizer::mesh_draw_t);
            constexpr size_t staging_stride = 2 * sizeof(matrix_struct_t);

            std::vector<vierkant::matrix_struct_t> matrix_data(2 * frame_context.dirty_drawable_indices.size());
            std::vector<vierkant::staging_copy_info_t> copy_transforms;
            uint32_t i = 0;

            for(auto idx: frame_context.dirty_drawable_indices)
            {
                assert(idx < frame_context.cull_result.drawables.size());
                const auto &drawable = frame_context.cull_result.drawables[idx];

                matrix_data[2 * i] = drawable.matrices;
                matrix_data[2 * i + 1] = drawable.last_matrices ? *drawable.last_matrices : drawable.matrices;

                vierkant::staging_copy_info_t copy_transform = {};
                copy_transform.num_bytes = staging_stride;
                copy_transform.data = matrix_data.data() + 2 * i;
                copy_transform.dst_buffer = params.mesh_draws;
                copy_transform.dst_offset = stride * idx;
                copy_transform.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
                copy_transform.dst_stage = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;
                copy_transforms.push_back(copy_transform);

                if(frame_context.indirect_draw_params_post.mesh_draws)
                {
                    // extra copy into post-meshdraws, not most elegant way
                    copy_transform.dst_buffer = frame_context.indirect_draw_params_post.mesh_draws;
                    copy_transforms.push_back(copy_transform);
                }
                i++;
            }
            vierkant::staging_copy(staging_context, copy_transforms);
        }
        frame_context.cmd_clear.submit(m_queue);
    };

    // pre-render will repeat all previous draw-calls
    frame_context.timings_map[G_BUFFER_LAST_VISIBLE] = m_g_renderer_main.last_frame_ms();

    auto cmd_buffer_pre = m_g_renderer_main.render(frame_context.g_buffer_main, frame_context.recycle_commands);
    vierkant::semaphore_submit_info_t g_buffer_semaphore_submit_info_pre = {};
    g_buffer_semaphore_submit_info_pre.semaphore = frame_context.timeline.handle();
    g_buffer_semaphore_submit_info_pre.wait_value = frame_context.current_semaphore_value + SemaphoreValue::PRE_RENDER;
    g_buffer_semaphore_submit_info_pre.wait_stage = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;
    g_buffer_semaphore_submit_info_pre.signal_value =
            frame_context.current_semaphore_value +
            (use_gpu_culling ? SemaphoreValue::G_BUFFER_LAST_VISIBLE : SemaphoreValue::G_BUFFER_ALL);
    frame_context.g_buffer_main.submit({cmd_buffer_pre}, m_queue, {g_buffer_semaphore_submit_info_pre});

    if(use_gpu_culling)
    {
        // generate depth-pyramid
        vierkant::create_depth_pyramid_params_t depth_pyramid_params = {};
        depth_pyramid_params.depth_map = frame_context.g_buffer_main.depth_attachment();
        depth_pyramid_params.queue = m_queue;
        depth_pyramid_params.query_pool = frame_context.query_pool;
        depth_pyramid_params.query_index_start = 2 * SemaphoreValue::DEPTH_PYRAMID;
        depth_pyramid_params.query_index_end = 2 * SemaphoreValue::DEPTH_PYRAMID + 1;
        depth_pyramid_params.semaphore_submit_info.semaphore = frame_context.timeline.handle();
        depth_pyramid_params.semaphore_submit_info.wait_value =
                frame_context.current_semaphore_value + SemaphoreValue::G_BUFFER_LAST_VISIBLE;
        depth_pyramid_params.semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        depth_pyramid_params.semaphore_submit_info.signal_value =
                frame_context.current_semaphore_value + SemaphoreValue::DEPTH_PYRAMID;
        frame_context.depth_pyramid = create_depth_pyramid(frame_context.gpu_cull_context, depth_pyramid_params);

        // post-render will perform actual culling
        m_g_renderer_post.draw_indirect_delegate = [this, &frame_context](Rasterizer::indirect_draw_bundle_t &params) {
            resize_indirect_draw_buffers(params.num_draws, frame_context.indirect_draw_params_post);
            params.draws_counts_out = frame_context.indirect_draw_params_post.draws_counts_out;
            frame_context.indirect_draw_params_post.mesh_draws = params.mesh_draws;

            // populate gpu-culling params
            vierkant::gpu_cull_params_t gpu_cull_params = {};
            gpu_cull_params.num_draws = params.num_draws;
            gpu_cull_params.camera = frame_context.cull_result.camera;
            gpu_cull_params.queue = m_queue;
            gpu_cull_params.query_pool = frame_context.query_pool;
            gpu_cull_params.query_index_start = 2 * SemaphoreValue::CULLING;
            gpu_cull_params.query_index_end = 2 * SemaphoreValue::CULLING + 1;
            gpu_cull_params.frustum_cull = frame_context.settings.frustum_culling;
            gpu_cull_params.occlusion_cull = frame_context.settings.occlusion_culling;
            gpu_cull_params.lod_enabled = frame_context.settings.enable_lod;
            gpu_cull_params.depth_pyramid = frame_context.depth_pyramid;
            gpu_cull_params.draws_in = frame_context.indirect_draw_params_main.draws_in;
            gpu_cull_params.mesh_draws_in = frame_context.indirect_draw_params_main.mesh_draws;
            gpu_cull_params.mesh_entries_in = frame_context.indirect_draw_params_main.mesh_entries;

            gpu_cull_params.draws_out_pre = frame_context.indirect_draw_params_main.draws_out;
            gpu_cull_params.draws_counts_out_pre = frame_context.indirect_draw_params_main.draws_counts_out;
            gpu_cull_params.draws_out_post = params.draws_out;
            gpu_cull_params.draws_counts_out_post = frame_context.indirect_draw_params_post.draws_counts_out;

            gpu_cull_params.semaphore_submit_info.semaphore = frame_context.timeline.handle();
            gpu_cull_params.semaphore_submit_info.wait_value =
                    frame_context.current_semaphore_value + SemaphoreValue::DEPTH_PYRAMID;
            gpu_cull_params.semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            gpu_cull_params.semaphore_submit_info.signal_value =
                    frame_context.current_semaphore_value + SemaphoreValue::CULLING;

            frame_context.stats.draw_cull_result = vierkant::gpu_cull(frame_context.gpu_cull_context, gpu_cull_params);
        };

        vierkant::semaphore_submit_info_t g_buffer_semaphore_submit_info = {};
        g_buffer_semaphore_submit_info.semaphore = frame_context.timeline.handle();
        g_buffer_semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        g_buffer_semaphore_submit_info.wait_value = frame_context.current_semaphore_value + SemaphoreValue::CULLING;
        g_buffer_semaphore_submit_info.signal_value =
                frame_context.current_semaphore_value + SemaphoreValue::G_BUFFER_ALL;

        frame_context.timings_map[G_BUFFER_ALL] = m_g_renderer_post.last_frame_ms();
        auto cmd_buffer = m_g_renderer_post.render(frame_context.g_buffer_post, frame_context.recycle_commands);
        frame_context.g_buffer_post.submit({cmd_buffer}, m_queue, {g_buffer_semaphore_submit_info});
    }

    frame_context.semaphore_value_done = SemaphoreValue::G_BUFFER_ALL;
    return frame_context.g_buffer_post;
}

vierkant::Framebuffer &PBRDeferred::lighting_pass(const cull_result_t &cull_result)
{
    size_t index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                   m_g_renderer_main.num_concurrent_frames();
    auto &frame_context = m_frame_contexts[index];

    frame_context.cmd_lighting.begin(0);
    vierkant::begin_label(frame_context.cmd_lighting.handle(), {"PBRDeferred::lighting_pass"});
    vierkant::ImagePtr occlusion_img = m_util_img_white;

    if(frame_context.settings.ambient_occlusion)
    {
        size_t last_index = (index - 1) % m_g_renderer_main.num_concurrent_frames();
        auto &last_frame_context = m_frame_contexts[last_index];

        if(frame_context.settings.use_ray_queries)
        {
            if(!frame_context.scene_acceleration_context)
            {
                frame_context.scene_acceleration_context = m_ray_builder.create_scene_acceleration_context();
            }

            RayBuilder::build_scene_acceleration_params_t build_scene_params = {};
            build_scene_params.scene = cull_result.scene;
            build_scene_params.use_compaction = false;
            build_scene_params.use_scene_assets = false;
            build_scene_params.previous_context = last_frame_context.scene_acceleration_context.get();
            frame_context.scene_ray_acceleration = m_ray_builder.build_scene_acceleration(
                    frame_context.scene_acceleration_context, build_scene_params);
        }

        vkCmdWriteTimestamp2(frame_context.cmd_lighting.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             frame_context.query_pool.get(), 2 * SemaphoreValue::AMBIENT_OCCLUSION);

        // ambient occlusion (optional)
        vierkant::ambient_occlusion_params_t ambient_occlusion_params = {};
        ambient_occlusion_params.use_ray_queries = frame_context.settings.use_ray_queries;
        ambient_occlusion_params.top_level = frame_context.scene_ray_acceleration.top_lvl.structure;
        ambient_occlusion_params.projection = cull_result.camera->projection_matrix();
        ambient_occlusion_params.camera_transform = cull_result.camera->transform;
        ambient_occlusion_params.normal_img = frame_context.g_buffer_post.color_attachment(G_BUFFER_NORMAL);
        ambient_occlusion_params.depth_img = frame_context.g_buffer_post.depth_attachment();
        ambient_occlusion_params.max_distance = frame_context.settings.max_ao_distance;
        ambient_occlusion_params.num_rays = 5;
        ambient_occlusion_params.commandbuffer = frame_context.cmd_lighting.handle();
        occlusion_img = vierkant::ambient_occlusion(frame_context.ambient_occlusion_context, ambient_occlusion_params);
        vkCmdWriteTimestamp2(frame_context.cmd_lighting.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             frame_context.query_pool.get(), 2 * SemaphoreValue::AMBIENT_OCCLUSION + 1);

        frame_context.internal_images.occlusion = occlusion_img;
    }

    environment_lighting_ubo_t ubo = {};
    ubo.camera_transform = mat4_cast(cull_result.camera->global_transform());
    ubo.inverse_projection = glm::inverse(cull_result.camera->projection_matrix());
    ubo.num_mip_levels = static_cast<int>(std::log2(m_conv_ggx->width()) + 1);
    ubo.environment_factor = frame_context.settings.environment_factor;
    ubo.num_lights = frame_context.cull_result.lights.size();
    frame_context.lighting_param_ubo->set_data(&ubo, sizeof(ubo));
    frame_context.lights_ubo->set_data(frame_context.cull_result.lights);

    // environment lighting-pass
    auto drawable = m_drawable_lighting_env;
    drawable.descriptors[0].buffers = {frame_context.lighting_param_ubo};
    drawable.descriptors[1].images = {frame_context.g_buffer_post.color_attachment(G_BUFFER_ALBEDO),
                                      frame_context.g_buffer_post.color_attachment(G_BUFFER_NORMAL),
                                      frame_context.g_buffer_post.color_attachment(G_BUFFER_EMISSION),
                                      frame_context.g_buffer_post.color_attachment(G_BUFFER_AO_ROUGH_METAL),
                                      occlusion_img,
                                      frame_context.g_buffer_post.color_attachment(G_BUFFER_MOTION),
                                      frame_context.g_buffer_post.depth_attachment(),
                                      m_brdf_lut};
    drawable.descriptors[2].images = {m_conv_lambert, m_conv_ggx};
    drawable.descriptors[3].buffers = {frame_context.lights_ubo};

    // stage, render, submit
    m_renderer_lighting.stage_drawable(drawable);

    vierkant::Rasterizer::rendering_info_t rendering_info = {};
    rendering_info.command_buffer = frame_context.cmd_lighting.handle();
    rendering_info.color_attachment_formats = {frame_context.lighting_buffer.color_attachment()->format().format};
    rendering_info.depth_attachment_format = frame_context.lighting_buffer.depth_attachment()->format().format;

    vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
    begin_rendering_info.commandbuffer = frame_context.cmd_lighting.handle();
    begin_rendering_info.use_depth_attachment = false;
    begin_rendering_info.clear_depth_attachment = false;

    vkCmdWriteTimestamp2(frame_context.cmd_lighting.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                         frame_context.query_pool.get(), 2 * SemaphoreValue::LIGHTING);
    frame_context.lighting_buffer.begin_rendering(begin_rendering_info);
    m_renderer_lighting.render(rendering_info);
    vkCmdEndRendering(frame_context.cmd_lighting.handle());

    // skybox rendering
    if(frame_context.settings.draw_skybox)
    {
        if(cull_result.scene->environment())
        {
            m_draw_context.draw_skybox(m_renderer_lighting, cull_result.scene->environment(), cull_result.camera);

            begin_rendering_info.clear_color_attachment = false;
            begin_rendering_info.use_depth_attachment = true;
            frame_context.lighting_buffer.begin_rendering(begin_rendering_info);
            m_renderer_lighting.render(rendering_info);
            vkCmdEndRendering(frame_context.cmd_lighting.handle());
        }
    }
    vkCmdWriteTimestamp2(frame_context.cmd_lighting.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                         frame_context.query_pool.get(), 2 * SemaphoreValue::LIGHTING + 1);

    vierkant::semaphore_submit_info_t lighting_semaphore_info = {};
    lighting_semaphore_info.semaphore = frame_context.timeline.handle();
    lighting_semaphore_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    lighting_semaphore_info.wait_value = frame_context.current_semaphore_value + SemaphoreValue::G_BUFFER_ALL;
    lighting_semaphore_info.signal_value = frame_context.current_semaphore_value + SemaphoreValue::LIGHTING;
    frame_context.semaphore_value_done = SemaphoreValue::LIGHTING;
    vierkant::end_label(frame_context.cmd_lighting.handle());
    frame_context.cmd_lighting.submit(m_queue, false, VK_NULL_HANDLE,
                                      {frame_context.scene_ray_acceleration.semaphore_info, lighting_semaphore_info});
    return frame_context.lighting_buffer;
}

vierkant::ImagePtr PBRDeferred::post_fx_pass(const CameraPtr &cam, const vierkant::ImagePtr &color,
                                             const vierkant::ImagePtr &depth)
{
    size_t frame_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                         m_g_renderer_main.num_concurrent_frames();
    size_t last_frame_index =
            (frame_index + m_g_renderer_main.num_concurrent_frames() - 1) % m_g_renderer_main.num_concurrent_frames();

    auto &frame_context = m_frame_contexts[frame_index];
    vierkant::ImagePtr output_img = color;

    vierkant::staging_copy_context_t staging_context = {};
    staging_context.command_buffer = frame_context.cmd_post_fx.handle();
    staging_context.staging_buffer = frame_context.staging_post_fx;

    // lighting/TAA are optional for sync
    auto semaphore_wait_value = frame_context.semaphore_value_done;

    size_t buffer_index = 0;

    // get next set of pingpong assets, increment index
    auto pingpong_render = [&frame_context, &buffer_index, &renderer = m_renderer_post_fx](
                                   vierkant::drawable_t &drawable, SemaphoreValue semaphore_value,
                                   const vierkant::Framebuffer &override_fb = {}) -> vierkant::ImagePtr {
        const vierkant::Framebuffer *framebuffer =
                override_fb ? &override_fb : &frame_context.post_fx_ping_pongs[buffer_index % 2];
        if(!override_fb) { buffer_index++; }
        auto cmd = frame_context.cmd_post_fx.handle();
        auto color_attachment = framebuffer->color_attachment();

        vierkant::begin_label(cmd, {crocore::to_lower(to_string(semaphore_value))});
        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, frame_context.query_pool.get(),
                             2 * semaphore_value);

        drawable.pipeline_format.scissor.extent.width = color_attachment->width();
        drawable.pipeline_format.scissor.extent.height = color_attachment->height();

        vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
        begin_rendering_info.commandbuffer = cmd;
        framebuffer->begin_rendering(begin_rendering_info);
        renderer.stage_drawable(drawable);

        vierkant::Rasterizer::rendering_info_t rendering_info = {};
        rendering_info.command_buffer = cmd;
        rendering_info.color_attachment_formats = {color_attachment->format().format};
        renderer.render(rendering_info);
        vkCmdEndRendering(cmd);

        vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, frame_context.query_pool.get(),
                             2 * semaphore_value + 1);
        vierkant::end_label(cmd);
        frame_context.semaphore_value_done = semaphore_value;
        color_attachment->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
        return color_attachment;
    };

    // begin command-buffer
    frame_context.cmd_post_fx.begin(0);
    vierkant::begin_label(frame_context.cmd_post_fx.handle(), {"PBRDeferred::post_fx_pass"});

    // TAA
    if(frame_context.settings.use_taa)
    {
        // assign history
        auto history_color = m_frame_contexts[last_frame_index].taa_buffer.color_attachment();
        auto history_depth = m_frame_contexts[last_frame_index].depth_map;

        auto drawable = m_drawable_taa;
        drawable.descriptors[0].images = {output_img, depth,
                                          frame_context.g_buffer_post.color_attachment(G_BUFFER_MOTION), history_color,
                                          history_depth};

        if(!drawable.descriptors[1].buffers.empty())
        {
            const auto &buf = drawable.descriptors[1].buffers.front();
            buf->barrier(frame_context.cmd_post_fx.handle(), VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         VK_ACCESS_2_TRANSFER_WRITE_BIT);

            vierkant::staging_copy_info_t staging_copy_info = {};
            staging_copy_info.data = &frame_context.camera_params;
            staging_copy_info.num_bytes = sizeof(camera_params_t);
            staging_copy_info.dst_buffer = buf;
            staging_copy_info.dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            staging_copy_info.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
            vierkant::staging_copy(staging_context, {staging_copy_info});
        }
        output_img = pingpong_render(drawable, SemaphoreValue::TAA, frame_context.taa_buffer);
    }

    // tonemap / bloom
    if(frame_context.settings.tonemap)
    {
        auto bloom_img = m_util_img_black;

        // generate bloom image
        if(frame_context.settings.bloom)
        {
            vkCmdWriteTimestamp2(frame_context.cmd_post_fx.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                 frame_context.query_pool.get(), 2 * SemaphoreValue::BLOOM);
            bloom_img = frame_context.bloom->apply(output_img, frame_context.cmd_post_fx.handle());
            vkCmdWriteTimestamp2(frame_context.cmd_post_fx.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                 frame_context.query_pool.get(), 2 * SemaphoreValue::BLOOM + 1);
            frame_context.semaphore_value_done = SemaphoreValue::BLOOM;
        }

        // motionblur
        auto motion_img = m_util_img_black;
        if(frame_context.settings.motionblur)
        {
            motion_img = frame_context.g_buffer_post.color_attachment(G_BUFFER_MOTION);
        }

        composition_ubo_t comp_ubo = {};
        comp_ubo.exposure = frame_context.settings.exposure;
        comp_ubo.gamma = frame_context.settings.gamma;

        using duration_t = std::chrono::duration<float>;
        comp_ubo.time_delta =
                duration_t(frame_context.timestamp - m_frame_contexts[last_frame_index].timestamp).count();
        comp_ubo.motionblur_gain = frame_context.settings.motionblur_gain;
        frame_context.composition_ubo->set_data(&comp_ubo, sizeof(composition_ubo_t));

        m_drawable_bloom.descriptors[0].images = {output_img, bloom_img, motion_img};
        m_drawable_bloom.descriptors[1].buffers = {frame_context.composition_ubo};
        output_img = pingpong_render(m_drawable_bloom, SemaphoreValue::TONEMAP);
    }

    // fxaa
    if(frame_context.settings.use_fxaa)
    {
        auto drawable = m_drawable_fxaa;
        drawable.descriptors[0].images = {output_img};
        output_img = pingpong_render(drawable, SemaphoreValue::FXAA);
    }

    // depth of field
    if(frame_context.settings.depth_of_field)
    {
        auto drawable = m_drawable_dof;
        drawable.descriptors[0].images = {output_img, depth};

        if(!drawable.descriptors[1].buffers.empty())
        {
            auto perspective_cam = std::dynamic_pointer_cast<vierkant::PerspectiveCamera>(cam);
            const auto &cam_params = perspective_cam->perspective_params;
            depth_of_field_params_t dof_params = {};
            dof_params.focal_distance = cam_params.focal_distance;
            dof_params.focal_length = cam_params.focal_length;
            dof_params.sensor_width = cam_params.sensor_width;
            dof_params.aperture = static_cast<float>(cam_params.aperture_size());
            dof_params.near = cam_params.clipping_distances.x;
            dof_params.far = cam_params.clipping_distances.y;
            vierkant::staging_copy_info_t staging_copy_info = {};
            staging_copy_info.data = &dof_params;
            staging_copy_info.num_bytes = sizeof(dof_params);
            staging_copy_info.dst_buffer = drawable.descriptors[1].buffers.front();
            staging_copy_info.dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            staging_copy_info.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
            vierkant::staging_copy(staging_context, {staging_copy_info});
        }
        output_img = pingpong_render(drawable, SemaphoreValue::DEFOCUS_BLUR);
    }

    // copy depthmap for next frame
    frame_context.g_buffer_main.depth_attachment()->copy_to(frame_context.depth_map,
                                                            frame_context.cmd_post_fx.handle());
    frame_context.g_buffer_main.depth_attachment()->transition_layout(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                                      frame_context.cmd_post_fx.handle());
    frame_context.depth_map->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, frame_context.cmd_post_fx.handle());

    vierkant::end_label(frame_context.cmd_post_fx.handle());
    frame_context.cmd_post_fx.end();

    if(frame_context.semaphore_value_done >= SemaphoreValue::TAA)
    {
        vierkant::semaphore_submit_info_t post_fx_semaphore_info = {};
        post_fx_semaphore_info.semaphore = frame_context.timeline.handle();
        post_fx_semaphore_info.wait_value = frame_context.current_semaphore_value + semaphore_wait_value;
        post_fx_semaphore_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        post_fx_semaphore_info.signal_value =
                frame_context.current_semaphore_value + frame_context.semaphore_value_done;
        frame_context.cmd_post_fx.submit(m_queue, false, VK_NULL_HANDLE, {post_fx_semaphore_info});
    }
    return output_img;
}

void PBRDeferred::set_environment(const ImagePtr &lambert, const ImagePtr &ggx)
{
    m_conv_lambert = lambert;
    m_conv_ggx = ggx;
}

void vierkant::PBRDeferred::resize_storage(vierkant::PBRDeferred::frame_context_t &frame_context,
                                           const glm::uvec2 &resolution, const glm::uvec2 &out_resolution)
{
    glm::uvec2 previous_size = {frame_context.g_buffer_post.extent().width,
                                frame_context.g_buffer_post.extent().height};
    glm::uvec2 previous_output_size = {frame_context.taa_buffer.extent().width,
                                       frame_context.taa_buffer.extent().height};

    frame_context.settings.resolution = glm::max(resolution, glm::uvec2(16));
    frame_context.settings.output_resolution = glm::max(out_resolution, glm::uvec2(16));

    VkExtent3D size = {frame_context.settings.resolution.x, frame_context.settings.resolution.y, 1};
    VkExtent3D upscaling_size = {frame_context.settings.output_resolution.x, frame_context.settings.output_resolution.y,
                                 1};

    VkViewport viewport = {};
    viewport.width = static_cast<float>(size.width);
    viewport.height = static_cast<float>(size.height);
    viewport.maxDepth = 1;

    m_g_renderer_main.viewport = viewport;
    m_g_renderer_post.viewport = viewport;
    m_renderer_lighting.viewport = viewport;

    // TAA and post-FX potentially upscaled
    viewport.width = static_cast<float>(upscaling_size.width);
    viewport.height = static_cast<float>(upscaling_size.height);
    m_renderer_post_fx.viewport = viewport;

    // internal resolution changed
    if(!frame_context.g_buffer_post || frame_context.g_buffer_post.color_attachment()->extent() != size)
    {
        // G-buffer (pre and post occlusion-culling)
        frame_context.g_buffer_main = create_g_buffer(m_device, size);
        frame_context.g_buffer_main.debug_label = {.text = "g_buffer_main"};

        auto renderpass_no_clear_depth =
                vierkant::create_renderpass(m_device, frame_context.g_buffer_main.attachments(), false, false);
        frame_context.g_buffer_post =
                vierkant::Framebuffer(m_device, frame_context.g_buffer_main.attachments(), renderpass_no_clear_depth);
        frame_context.g_buffer_post.debug_label = {.text = "g_buffer_post"};
        frame_context.g_buffer_post.clear_color = {{0.f, 0.f, 0.f, 0.f}};

        auto depth_fmt = frame_context.g_buffer_main.depth_attachment()->format();
        depth_fmt.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        frame_context.depth_map = vierkant::Image::create(m_device, depth_fmt);

        // init lighting framebuffer
        vierkant::attachment_map_t lighting_attachments;
        vierkant::Image::Format hdr_attachment_info = {};
        hdr_attachment_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        hdr_attachment_info.format = m_hdr_format;
        hdr_attachment_info.extent = size;
        lighting_attachments[vierkant::AttachmentType::Color] = {
                vierkant::Image::create(m_device, hdr_attachment_info)};

        // use depth from g_buffer
        lighting_attachments[vierkant::AttachmentType::DepthStencil] = {frame_context.g_buffer_post.depth_attachment()};
        frame_context.lighting_buffer = vierkant::Framebuffer(m_device, lighting_attachments);

        // resize ambient occlusion context
        auto ao_resolution = glm::max(glm::vec2(frame_context.settings.resolution) / 2.f, glm::vec2(1.f));
        frame_context.ambient_occlusion_context =
                vierkant::create_ambient_occlusion_context(m_device, ao_resolution, m_pipeline_cache);

        m_logger->trace("internal resolution: {} x {} -> {} x {}", previous_size.x, previous_size.y, resolution.x,
                        resolution.y);
        frame_context.recycle_commands = false;
    }

    // upscaling resolution changed
    if(!frame_context.taa_buffer || frame_context.taa_buffer.color_attachment()->extent() != upscaling_size)
    {
        // post-fx buffers with optional upscaling
        vierkant::Framebuffer::create_info_t post_fx_buffer_info = {};
        post_fx_buffer_info.size = upscaling_size;
        post_fx_buffer_info.color_attachment_format.usage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        post_fx_buffer_info.color_attachment_format.extent = upscaling_size;
        post_fx_buffer_info.color_attachment_format.format = m_hdr_format;
        frame_context.taa_buffer = vierkant::Framebuffer(m_device, post_fx_buffer_info);

        // create post_fx ping pong buffers and renderers
        for(auto &post_fx_ping_pong: frame_context.post_fx_ping_pongs)
        {
            post_fx_ping_pong = vierkant::Framebuffer(m_device, post_fx_buffer_info);
            post_fx_ping_pong.clear_color = {{0.f, 0.f, 0.f, 0.f}};
        }

        // create bloom
        Bloom::create_info_t bloom_info = {};
        bloom_info.size = upscaling_size;
        bloom_info.color_format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        bloom_info.size.width = std::max(1U, bloom_info.size.width / 2);
        bloom_info.size.height = std::max(1U, bloom_info.size.height / 2);
        bloom_info.num_blur_iterations = 3;
        bloom_info.command_pool = m_command_pool;
        bloom_info.pipeline_cache = m_pipeline_cache;
        frame_context.bloom = Bloom::create(m_device, bloom_info);

        m_logger->trace("output resolution: {} x {} -> {} x {}", previous_output_size.x, previous_output_size.y,
                        out_resolution.x, out_resolution.y);
    }

    frame_context.internal_images.albedo = frame_context.g_buffer_main.color_attachment(G_BUFFER_ALBEDO);
    frame_context.internal_images.normals = frame_context.g_buffer_main.color_attachment(G_BUFFER_NORMAL);
    frame_context.internal_images.emission = frame_context.g_buffer_main.color_attachment(G_BUFFER_EMISSION);
    frame_context.internal_images.ao_rough_metal =
            frame_context.g_buffer_main.color_attachment(G_BUFFER_AO_ROUGH_METAL);
    frame_context.internal_images.motion = frame_context.g_buffer_main.color_attachment(G_BUFFER_MOTION);
    frame_context.internal_images.object_ids = frame_context.g_buffer_main.color_attachment(G_BUFFER_OBJECT_ID);
    frame_context.internal_images.lighting = frame_context.lighting_buffer.color_attachment();

    frame_context.internal_images.bsdf_lut = m_brdf_lut;
    frame_context.internal_images.environment_diffuse = m_conv_lambert;
    frame_context.internal_images.environment_specular = m_conv_ggx;
}

void PBRDeferred::resize_indirect_draw_buffers(uint32_t num_draws, Rasterizer::indirect_draw_bundle_t &params)
{
    // reserve space for indirect drawing-commands
    const size_t num_bytes = std::max<size_t>(num_draws * sizeof(Rasterizer::indexed_indirect_command_t), 1ul << 20);

    if(!params.draws_in || params.draws_in->num_bytes() < num_bytes)
    {
        params.draws_in = vierkant::Buffer::create(
                m_device, nullptr, num_bytes,
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY);
    }

    if(!params.draws_counts_out)
    {
        constexpr uint32_t max_batches = 4096;
        params.draws_counts_out = vierkant::Buffer::create(
                m_device, nullptr, max_batches * sizeof(uint32_t),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY);
    }
    params.num_draws = num_draws;
}

void PBRDeferred::update_timing(frame_context_t &frame_context)
{
    timings_t &timings_result = frame_context.stats.timings;
    frame_context.stats.timestamp = frame_context.timestamp;

    const size_t query_count = 2 * SemaphoreValue::MAX_VALUE;

    uint64_t timestamps[query_count] = {};
    auto query_result = vkGetQueryPoolResults(m_device->handle(), frame_context.query_pool.get(), 0, query_count,
                                              sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

    double timing_millis[SemaphoreValue::MAX_VALUE] = {};

    if(query_result == VK_SUCCESS || query_result == VK_NOT_READY)
    {
        auto timestamp_period = m_device->properties().core.limits.timestampPeriod;

        for(uint32_t i = G_BUFFER_LAST_VISIBLE; i <= frame_context.semaphore_value_done; ++i)
        {
            auto val = SemaphoreValue(i);
            auto measurement = vierkant::timestamp_millis(timestamps, val, timestamp_period);
            timing_millis[i] = measurement;
        }
    }

    timings_result.g_buffer_pre_ms = frame_context.timings_map[SemaphoreValue::G_BUFFER_LAST_VISIBLE].count();
    timings_result.depth_pyramid_ms = timing_millis[SemaphoreValue::DEPTH_PYRAMID];
    timings_result.culling_ms = timing_millis[SemaphoreValue::CULLING];
    timings_result.g_buffer_post_ms = frame_context.timings_map[SemaphoreValue::G_BUFFER_ALL].count();
    timings_result.lighting_ms = timing_millis[SemaphoreValue::LIGHTING];
    timings_result.ambient_occlusion_ms = timing_millis[SemaphoreValue::AMBIENT_OCCLUSION];
    timings_result.taa_ms = timing_millis[SemaphoreValue::TAA];
    timings_result.fxaa_ms = timing_millis[SemaphoreValue::FXAA];
    timings_result.bloom_ms = timing_millis[SemaphoreValue::BLOOM];
    timings_result.tonemap_ms = timing_millis[SemaphoreValue::TONEMAP];
    timings_result.depth_of_field_ms = timing_millis[SemaphoreValue::DEFOCUS_BLUR];

    timings_result.total_ms = timings_result.g_buffer_pre_ms + timings_result.depth_pyramid_ms +
                              timings_result.culling_ms + timings_result.g_buffer_post_ms + timings_result.lighting_ms +
                              timings_result.taa_ms + timings_result.bloom_ms + timings_result.tonemap_ms;

    frame_context.stats.timings = timings_result;

    m_statistics.push_back(frame_context.stats);
    while(m_statistics.size() > frame_context.settings.timing_history_size) { m_statistics.pop_front(); }

    // reset query-pool
    vkResetQueryPool(m_device->handle(), frame_context.query_pool.get(), 0, query_count);
    frame_context.timings_map.clear();
}

const PBRDeferred::image_bundle_t &PBRDeferred::image_bundle() const
{
    size_t frame_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                         m_g_renderer_main.num_concurrent_frames();
    auto &frame_context = m_frame_contexts[frame_index];
    return frame_context.internal_images;
}

std::vector<uint16_t> PBRDeferred::pick(const glm::vec2 &normalized_coord, const glm::vec2 &normalized_size)
{
    size_t frame_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                         m_g_renderer_main.num_concurrent_frames();
    auto &frame_context = m_frame_contexts[frame_index];
    const auto &img_bundle = frame_context.internal_images;

    frame_context.cmd_copy_object_id.begin();

    auto img_size = glm::vec2(img_bundle.object_ids->width(), img_bundle.object_ids->height());
    glm::vec2 adjusted_pos = normalized_coord * img_size;
    glm::uvec2 adjusted_size = glm::max(normalized_size * img_size, {1, 1});
    adjusted_pos = glm::clamp(adjusted_pos, glm::vec2(0), img_size - glm::vec2(1));

    VkExtent3D img_extent = {adjusted_size.x, adjusted_size.y, 1};
    VkOffset3D img_offset = {static_cast<int32_t>(adjusted_pos.x), static_cast<int32_t>(adjusted_pos.y), 0};

    uint32_t num_object_ids = img_extent.width * img_extent.height;
    uint32_t num_bytes = std::max<uint32_t>(sizeof(uint16_t) * num_object_ids, 512);
    auto buf = vierkant::Buffer::create(m_device, nullptr, num_bytes, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
                                        VMA_MEMORY_USAGE_CPU_ONLY);
    auto prev_layout = img_bundle.object_ids->image_layout();
    img_bundle.object_ids->copy_to(buf, frame_context.cmd_copy_object_id.handle(), 0, img_offset, img_extent);
    img_bundle.object_ids->transition_layout(prev_layout, frame_context.cmd_copy_object_id.handle());

    // wait for frame, copy draw-ids
    vierkant::semaphore_submit_info_t semaphore_info = {};
    semaphore_info.semaphore = frame_context.timeline.handle();
    semaphore_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    semaphore_info.wait_value = frame_context.current_semaphore_value + frame_context.semaphore_value_done;
    frame_context.cmd_copy_object_id.submit(m_queue, true, VK_NULL_HANDLE, {semaphore_info});

    std::unordered_set<uint16_t> value_set;
    auto ptr = static_cast<const uint16_t *>(buf->map());
    for(uint32_t i = 0; i < num_object_ids; ++i)
    {
        if(ptr[i]) { value_set.insert(std::numeric_limits<uint16_t>::max() - ptr[i]); }
    }
    return {value_set.begin(), value_set.end()};
}

bool operator==(const PBRDeferred::settings_t &lhs, const PBRDeferred::settings_t &rhs)
{
    if(lhs.resolution != rhs.resolution) { return false; }
    if(lhs.output_resolution != rhs.output_resolution) { return false; }
    if(lhs.disable_material != rhs.disable_material) { return false; }
    if(lhs.debug_draw_ids != rhs.debug_draw_ids) { return false; }
    if(lhs.frustum_culling != rhs.frustum_culling) { return false; }
    if(lhs.occlusion_culling != rhs.occlusion_culling) { return false; }
    if(lhs.enable_lod != rhs.enable_lod) { return false; }
    if(lhs.tesselation != rhs.tesselation) { return false; }
    if(lhs.wireframe != rhs.wireframe) { return false; }
    if(lhs.draw_skybox != rhs.draw_skybox) { return false; }
    if(lhs.use_fxaa != rhs.use_fxaa) { return false; }
    if(lhs.use_taa != rhs.use_taa) { return false; }
    if(lhs.environment_factor != rhs.environment_factor) { return false; }
    if(lhs.ambient_occlusion != rhs.ambient_occlusion) { return false; }
    if(lhs.max_ao_distance != rhs.max_ao_distance) { return false; }
    if(lhs.tonemap != rhs.tonemap) { return false; }
    if(lhs.bloom != rhs.bloom) { return false; }
    if(lhs.motionblur != rhs.motionblur) { return false; }
    if(lhs.motionblur_gain != rhs.motionblur_gain) { return false; }
    if(lhs.gamma != rhs.gamma) { return false; }
    if(lhs.exposure != rhs.exposure) { return false; }
    if(lhs.indirect_draw != rhs.indirect_draw) { return false; }
    if(lhs.use_meshlet_pipeline != rhs.use_meshlet_pipeline) { return false; }
    if(lhs.use_ray_queries != rhs.use_ray_queries) { return false; }
    if(lhs.timing_history_size != rhs.timing_history_size) { return false; }
    if(lhs.depth_of_field != rhs.depth_of_field) { return false; }
    return true;
    //    return memcmp(&lhs, &rhs, sizeof(PBRDeferred::settings_t)) == 0;
}

}// namespace vierkant