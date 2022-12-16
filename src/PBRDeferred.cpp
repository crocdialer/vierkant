//
// Created by crocdialer on 6/19/20.
//

#include <crocore/gaussian.hpp>

#include <vierkant/PBRDeferred.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/cubemap_utils.hpp>
#include <vierkant/culling.hpp>
#include <vierkant/punctual_light.hpp>
#include <vierkant/shaders.hpp>

namespace vierkant
{

inline float halton(uint32_t index, uint32_t base)
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

PBRDeferred::PBRDeferred(const DevicePtr &device, const create_info_t &create_info) : m_device(device)
{
    m_logger = create_info.logger_name.empty() ? spdlog::default_logger() : spdlog::get(create_info.logger_name);
    m_logger->debug("PBRDeferred initialized");

    m_queue = create_info.queue ? create_info.queue : device->queue();

    m_command_pool = vierkant::create_command_pool(m_device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    m_pipeline_cache =
            create_info.pipeline_cache ? create_info.pipeline_cache : vierkant::PipelineCache::create(device);

    m_frame_assets.resize(create_info.num_frames_in_flight);

    for(auto &asset: m_frame_assets)
    {
        resize_storage(asset, create_info.settings.resolution);

        asset.g_buffer_camera_ubo = vierkant::Buffer::create(
                m_device, nullptr, sizeof(glm::vec2), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.lighting_param_ubo =
                vierkant::Buffer::create(device, nullptr, sizeof(environment_lighting_ubo_t),
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.lights_ubo = vierkant::Buffer::create(device, nullptr, sizeof(vierkant::lightsource_ubo_t),
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.composition_ubo =
                vierkant::Buffer::create(device, nullptr, sizeof(composition_ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU);

        asset.query_pool = vierkant::create_query_pool(m_device, SemaphoreValue::MAX_VALUE, VK_QUERY_TYPE_TIMESTAMP);

        asset.gpu_cull_context = vierkant::create_gpu_cull_context(device);

        asset.staging_buffer = vierkant::Buffer::create(m_device, nullptr, 1U << 20U,
                                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        VMA_MEMORY_USAGE_CPU_ONLY);
    }

    // create renderer for g-buffer-pass
    vierkant::Renderer::create_info_t render_create_info = {};
    render_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    render_create_info.queue = create_info.queue;
    render_create_info.sample_count = create_info.sample_count;
    render_create_info.viewport.width = static_cast<float>(create_info.settings.resolution.x);
    render_create_info.viewport.height = static_cast<float>(create_info.settings.resolution.y);
    render_create_info.pipeline_cache = m_pipeline_cache;
    render_create_info.indirect_draw = true;
    render_create_info.enable_mesh_shader = true;
    m_g_renderer_main = vierkant::Renderer(device, render_create_info);
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
}

PBRDeferred::~PBRDeferred()
{
    for(auto &frame_asset: m_frame_assets){ frame_asset.timeline.wait(frame_asset.semaphore_value_done); }
}

PBRDeferredPtr PBRDeferred::create(const DevicePtr &device, const create_info_t &create_info)
{
    return vierkant::PBRDeferredPtr(new PBRDeferred(device, create_info));
}

void PBRDeferred::update_recycling(const SceneConstPtr &scene, const CameraPtr &cam, frame_asset_t &frame_asset)
{
    std::unordered_set<vierkant::MeshConstPtr> meshes;
    std::unordered_map<vierkant::id_entry_key_t, size_t, vierkant::id_entry_key_hash_t> transform_hashes;

    bool materials_unchanged = true;
    bool objects_unchanged = true;
    frame_asset.dirty_drawable_indices.clear();

    size_t scene_hash = 0;
    auto view = scene->registry()->view<vierkant::Object3D *, vierkant::MeshPtr>();

    for(const auto &[entity, object, mesh]: view.each())
    {
        bool transform_update = false;

        meshes.insert(mesh);
        crocore::hash_combine(scene_hash, object);

        bool animation_update = !mesh->node_animations.empty() && !mesh->root_bone && !mesh->morph_buffer &&
                                object->has_component<animation_state_t>();

        // entry animation transforms
        std::vector<glm::mat4> node_matrices;

        if(animation_update)
        {
            const auto &animation_state = object->get_component<animation_state_t>();
            const auto &animation = mesh->node_animations[animation_state.index];
            vierkant::nodes::build_node_matrices_bfs(mesh->root_node, animation, animation_state.current_time,
                                                     node_matrices);
        }

        for(uint32_t i = 0; i < mesh->entries.size(); ++i)
        {
            const auto &entry = mesh->entries[i];
            crocore::hash_combine(scene_hash, entry.enabled);

            id_entry_key_t key = {object->id(), i};
            auto it = m_entry_matrix_cache.find(key);

            size_t transform_hash = 0;
            crocore::hash_combine(transform_hash, object->transform * entry.transform);
            transform_hashes[key] = transform_hash;

            auto hash_it = frame_asset.transform_hashes.find(key);
            transform_update = transform_update ||
                               (hash_it != frame_asset.transform_hashes.end() && hash_it->second != transform_hash);

            if((transform_update || animation_update) && frame_asset.cull_result.index_map.contains(key))
            {
                // combine mesh- with entry-transform
                uint32_t drawable_index = frame_asset.cull_result.index_map.at(key);
                frame_asset.dirty_drawable_indices.insert(drawable_index);

                auto &drawable = frame_asset.cull_result.drawables[drawable_index];
                drawable.matrices.modelview = node_matrices.empty() ? object->global_transform() :
                                              object->global_transform() * node_matrices[entry.node_index];
                drawable.matrices.normal = glm::inverseTranspose(drawable.matrices.modelview);
                drawable.last_matrices =
                        it != m_entry_matrix_cache.end() ? it->second : std::optional<matrix_struct_t>();

                m_entry_matrix_cache[key] = drawable.matrices;
            }
        }
    }

    frame_asset.transform_hashes = std::move(transform_hashes);

    if(scene_hash != frame_asset.scene_hash)
    {
        objects_unchanged = false;
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
    frame_asset.recycle_commands =
            objects_unchanged && materials_unchanged && !need_culling && frame_asset.settings == settings;

    frame_asset.settings = settings;
}

SceneRenderer::render_result_t PBRDeferred::render_scene(Renderer &renderer, const SceneConstPtr &scene,
                                                         const CameraPtr &cam, const std::set<std::string> &tags)
{
    // reference to current frame-assets
    auto &frame_asset = m_frame_assets[m_g_renderer_main.current_index()];
    frame_asset.timestamp = std::chrono::steady_clock::now();

    // retrieve last performance-query, start new
    update_timing(frame_asset);

    // determine if we can re-use commandbuffers, buffers, etc.
    update_recycling(scene, cam, frame_asset);

    if(!frame_asset.recycle_commands)
    {
        // flush outdated transform-cache
        m_entry_matrix_cache.clear();

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

    SceneRenderer::render_result_t ret = {};
    ret.num_draws = frame_asset.cull_result.drawables.size();
    ret.num_frustum_culled = frame_asset.stats.draw_cull_result.num_frustum_culled;
    ret.num_occlusion_culled = frame_asset.stats.draw_cull_result.num_occlusion_culled;
    ret.num_distance_culled = frame_asset.stats.draw_cull_result.num_distance_culled;

    vierkant::semaphore_submit_info_t semaphore_submit_info = {};
    semaphore_submit_info.semaphore = frame_asset.timeline.handle();
    semaphore_submit_info.wait_value = frame_asset.semaphore_value_done;
    semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    ret.semaphore_infos = {semaphore_submit_info};
    return ret;
}

void PBRDeferred::update_matrix_history(frame_asset_t &frame_asset)
{
    matrix_cache_t new_entry_matrix_cache;

    // cache/collect bone-matrices
    std::unordered_map<entt::entity, size_t> bone_buffer_offsets;
    std::vector<glm::mat4> all_bones_matrices;

    // cache/collect morph-params
    //! morph_params_t contains information to access a morph-target buffer
    struct alignas(16) morph_params_t
    {
        uint32_t morph_count = 0;
        uint32_t base_vertex = 0;
        uint32_t vertex_count = 0;
        float weights[61] = {};
    };
    using morph_buffer_offset_mapt_t = std::unordered_map<id_entry_key_t, size_t, id_entry_key_hash_t>;
    morph_buffer_offset_mapt_t morph_buffer_offsets;
    std::vector<morph_params_t> all_morph_params;

    size_t last_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                        m_g_renderer_main.num_concurrent_frames();
    auto &last_frame_asset = m_frame_assets[last_index];

    auto view = frame_asset.cull_result.scene->registry()->view<vierkant::MeshPtr, vierkant::animation_state_t>();

    for(const auto &[entity, mesh, animation_state]: view.each())
    {
        const auto &animation = mesh->node_animations[animation_state.index];

        if(mesh->root_bone)
        {
            std::vector<glm::mat4> bones_matrices;
            vierkant::nodes::build_node_matrices_bfs(mesh->root_bone,
                                                     animation,
                                                     animation_state.current_time,
                                                     bones_matrices);

            // keep track of offset
            bone_buffer_offsets[entity] = all_bones_matrices.size() * sizeof(glm::mat4);
            all_bones_matrices.insert(all_bones_matrices.end(), bones_matrices.begin(), bones_matrices.end());
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
                id_entry_key_t key = {static_cast<uint32_t>(entity), i};
                const auto weights = node_morph_weights[entry.node_index];

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

    if(!frame_asset.bone_buffer)
    {
        frame_asset.bone_buffer = vierkant::Buffer::create(
                m_device, all_bones_matrices, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    else{ frame_asset.bone_buffer->set_data(all_bones_matrices); }

    if(!frame_asset.morph_param_buffer)
    {
        frame_asset.morph_param_buffer = vierkant::Buffer::create(
                m_device, all_morph_params, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }
    else{ frame_asset.morph_param_buffer->set_data(all_morph_params); }

    if(!frame_asset.recycle_commands)
    {
        // insert previous matrices from cache, if any
        for(auto &drawable: frame_asset.cull_result.drawables)
        {
            auto entity = entt::entity(frame_asset.cull_result.entity_map[drawable.id]);

            // search previous matrices
            id_entry_key_t key = {static_cast<uint32_t>(entity), drawable.entry_index};
            auto it = m_entry_matrix_cache.find(key);
            if(it != m_entry_matrix_cache.end()){ drawable.last_matrices = it->second; }

            // descriptors for bone buffers, if necessary
            if(drawable.mesh && drawable.mesh->root_bone)
            {
                uint32_t buffer_offset = bone_buffer_offsets[entity];

                vierkant::descriptor_t desc_bones = {};
                desc_bones.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_bones.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
                desc_bones.buffers = {frame_asset.bone_buffer};
                desc_bones.buffer_offsets = {buffer_offset};
                drawable.descriptors[Renderer::BINDING_BONES] = desc_bones;

                if(last_frame_asset.bone_buffer &&
                   last_frame_asset.bone_buffer->num_bytes() == frame_asset.bone_buffer->num_bytes())
                {
                    desc_bones.buffers = {last_frame_asset.bone_buffer};
                }
                drawable.descriptors[Renderer::BINDING_PREVIOUS_BONES] = desc_bones;
            }

            if(drawable.mesh && drawable.mesh->morph_buffer)
            {
                //! morph_params_t contains information to access a morph-target buffer
                uint32_t buffer_offset = morph_buffer_offsets[key];

                // use combined buffer
                vierkant::descriptor_t desc_morph_params = {};
                desc_morph_params.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_morph_params.stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
                desc_morph_params.buffers = {frame_asset.morph_param_buffer};
                desc_morph_params.buffer_offsets = {buffer_offset};
                drawable.descriptors[Renderer::BINDING_MORPH_PARAMS] = desc_morph_params;

                if(last_frame_asset.morph_param_buffer &&
                   last_frame_asset.morph_param_buffer->num_bytes() == frame_asset.morph_param_buffer->num_bytes())
                {
                    desc_morph_params.buffers = {last_frame_asset.morph_param_buffer};
                }
                drawable.descriptors[Renderer::BINDING_PREVIOUS_MORPH_PARAMS] = desc_morph_params;
            }

            // store current matrices
            new_entry_matrix_cache[key] = drawable.matrices;
        }
        m_entry_matrix_cache = std::move(new_entry_matrix_cache);
    }
}

vierkant::Framebuffer &PBRDeferred::geometry_pass(cull_result_t &cull_result)
{
    auto &frame_asset = m_frame_assets[m_g_renderer_main.current_index()];

    size_t last_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                        m_g_renderer_main.num_concurrent_frames();
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

    glm::mat4 projectionT = transpose(cull_result.camera->projection_matrix());
    glm::vec4 frustumX = projectionT[3] + projectionT[0];// x + w < 0
    frustumX /= glm::length(frustumX.xyz());
    glm::vec4 frustumY = projectionT[3] + projectionT[1];// y + w < 0
    frustumY /= glm::length(frustumY.xyz());
    frame_asset.camera_params.frustum = {frustumX.x, frustumX.z, frustumY.y, frustumY.z};

    camera_params_t cameras[2] = {frame_asset.camera_params, last_frame_asset.camera_params};
    frame_asset.g_buffer_camera_ubo->set_data(&cameras, sizeof(cameras));

    // decide on indirect rendering-path
    bool use_gpu_culling = frame_asset.settings.indirect_draw &&
                           (frame_asset.settings.frustum_culling || frame_asset.settings.occlusion_culling ||
                            frame_asset.settings.enable_lod);

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
                    drawable.descriptors[Renderer::BINDING_MESH_DRAWS].stage_flags =
                            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                    camera_desc.stage_flags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                }
            }

            // attribute/binding descriptions obsolete here
            drawable.pipeline_format.attribute_descriptions.clear();
            drawable.pipeline_format.binding_descriptions.clear();

            const bool use_meshlet_pipeline = drawable.mesh->meshlets && frame_asset.settings.use_meshlet_pipeline &&
                                              !drawable.mesh->morph_buffer && !drawable.mesh->root_bone;

            if(use_meshlet_pipeline)
            {
                shader_flags |= PROP_MESHLETS;
                camera_desc.stage_flags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
            }

            // check if morph-targets are available
            if(drawable.mesh->morph_buffer){ shader_flags |= PROP_MORPH_TARGET; }

            // select shader-stages from cache
            auto stage_it = m_g_buffer_shader_stages.find(shader_flags);

            // fallback to default if not found
            if(stage_it != m_g_buffer_shader_stages.end())
            {
                drawable.pipeline_format.shader_stages = stage_it->second;
            }
            else{ drawable.pipeline_format.shader_stages = m_g_buffer_shader_stages[PROP_DEFAULT]; }

            // set attachment count
            drawable.pipeline_format.attachment_count = G_BUFFER_SIZE;

            // disable blending
            drawable.pipeline_format.blend_state.blendEnable = false;
            drawable.pipeline_format.depth_test = true;
            drawable.pipeline_format.depth_write = true;

            // optional wireframe rendering
            drawable.pipeline_format.polygon_mode =
                    frame_asset.settings.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

            // add descriptor for a jitter-offset
            drawable.descriptors[Renderer::BINDING_JITTER_OFFSET] = camera_desc;
        }
        // stage drawables
        m_g_renderer_main.stage_drawables(cull_result.drawables);
        if(use_gpu_culling){ m_g_renderer_post.stage_drawables(cull_result.drawables); }
    }

    // apply current settings for both renderers
    m_g_renderer_main.disable_material = m_g_renderer_post.disable_material = frame_asset.settings.disable_material;
    m_g_renderer_main.debug_draw_ids = m_g_renderer_post.debug_draw_ids = frame_asset.settings.debug_draw_ids;
    m_g_renderer_main.indirect_draw = m_g_renderer_post.indirect_draw = frame_asset.settings.indirect_draw;
    m_g_renderer_main.use_mesh_shader = m_g_renderer_post.use_mesh_shader = frame_asset.settings.use_meshlet_pipeline;

    // draw last visible objects
    m_g_renderer_main.draw_indirect_delegate = [this, &frame_asset,
            use_gpu_culling](Renderer::indirect_draw_bundle_t &params)
    {
        frame_asset.clear_cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
        frame_asset.clear_cmd_buffer.begin();

        // base/first timestamp
        vkCmdWriteTimestamp2(frame_asset.clear_cmd_buffer.handle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             frame_asset.query_pool.get(), 0);

        resize_indirect_draw_buffers(params.num_draws, frame_asset.indirect_draw_params_main);
        frame_asset.indirect_draw_params_main.draws_out = params.draws_out;
        frame_asset.indirect_draw_params_main.mesh_draws = params.mesh_draws;
        frame_asset.indirect_draw_params_main.mesh_entries = params.mesh_entries;

        std::vector<VkBufferMemoryBarrier2> barriers;

        if(params.num_draws && !frame_asset.recycle_commands)
        {
            auto drawbuffer = use_gpu_culling ? frame_asset.indirect_draw_params_main.draws_in
                                              : frame_asset.indirect_draw_params_main.draws_out;
            params.draws_in->copy_to(drawbuffer, frame_asset.clear_cmd_buffer.handle());

            frame_asset.staging_buffer->set_data(nullptr, params.mesh_draws->num_bytes());

            VkBufferMemoryBarrier2 barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.buffer = drawbuffer->handle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            barriers.push_back(barrier);

            if(use_gpu_culling && !params.draws_counts_out)
            {
                params.draws_counts_out = frame_asset.indirect_draw_params_main.draws_counts_out;
                vkCmdFillBuffer(frame_asset.clear_cmd_buffer.handle(),
                                frame_asset.indirect_draw_params_main.draws_counts_out->handle(), 0, VK_WHOLE_SIZE, 0);

                barrier.buffer = frame_asset.indirect_draw_params_main.draws_counts_out->handle();
                barriers.push_back(barrier);
            }
        }
        else if(params.num_draws && !frame_asset.dirty_drawable_indices.empty())
        {
            constexpr size_t stride = sizeof(Renderer::mesh_draw_t);
            constexpr size_t staging_stride = 2 * sizeof(matrix_struct_t);

            const size_t num_staging_bytes = std::max(staging_stride * frame_asset.dirty_drawable_indices.size(),
                                                      frame_asset.staging_buffer->num_bytes());
            frame_asset.staging_buffer->set_data(nullptr, num_staging_bytes);
            size_t staging_offset = 0;

            auto staging_ptr = static_cast<uint8_t *>(frame_asset.staging_buffer->map());
            assert(staging_ptr);
            std::vector<VkBufferCopy2> copy_regions;
            copy_regions.reserve(frame_asset.dirty_drawable_indices.size());

            for(auto idx: frame_asset.dirty_drawable_indices)
            {
                assert(idx < frame_asset.cull_result.drawables.size());
                const auto &drawable = frame_asset.cull_result.drawables[idx];

                VkBufferCopy2 copy = {};
                copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
                copy.size = staging_stride;
                copy.srcOffset = staging_offset;
                copy.dstOffset = stride * idx;
                copy_regions.push_back(copy);

                vierkant::matrix_struct_t matrices[2] = {};
                matrices[0] = drawable.matrices;
                matrices[1] = drawable.last_matrices ? *drawable.last_matrices : drawable.matrices;
                memcpy(staging_ptr + staging_offset, matrices, sizeof(matrices));
                staging_offset += staging_stride;
            }
            frame_asset.staging_buffer->unmap();

            VkCopyBufferInfo2 copy_info2 = {};
            copy_info2.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
            copy_info2.srcBuffer = frame_asset.staging_buffer->handle();
            copy_info2.dstBuffer = params.mesh_draws->handle();
            copy_info2.regionCount = copy_regions.size();
            copy_info2.pRegions = copy_regions.data();
            vkCmdCopyBuffer2(frame_asset.clear_cmd_buffer.handle(), &copy_info2);

            VkBufferMemoryBarrier2 barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.buffer = params.mesh_draws->handle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            barriers.push_back(barrier);

            // TODO: improve placement, maybe try to share one(1) meshdraw-buffer for main/post rendering
            if(frame_asset.indirect_draw_params_post.mesh_draws)
            {
                copy_info2.dstBuffer = frame_asset.indirect_draw_params_post.mesh_draws->handle();
                vkCmdCopyBuffer2(frame_asset.clear_cmd_buffer.handle(), &copy_info2);
                barrier.buffer = frame_asset.indirect_draw_params_post.mesh_draws->handle();
                barriers.push_back(barrier);
            }
        }

        if(!barriers.empty())
        {
            VkDependencyInfo dependency_info = {};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.bufferMemoryBarrierCount = barriers.size();
            dependency_info.pBufferMemoryBarriers = barriers.data();
            vkCmdPipelineBarrier2(frame_asset.clear_cmd_buffer.handle(), &dependency_info);
        }
        frame_asset.clear_cmd_buffer.submit(m_queue);
    };

    // pre-render will repeat all previous draw-calls
    frame_asset.timings_map[G_BUFFER_LAST_VISIBLE] = m_g_renderer_main.last_frame_ms();

    auto cmd_buffer_pre = m_g_renderer_main.render(frame_asset.g_buffer_pre, frame_asset.recycle_commands);
    vierkant::semaphore_submit_info_t g_buffer_semaphore_submit_info_pre = {};
    g_buffer_semaphore_submit_info_pre.semaphore = frame_asset.timeline.handle();
    g_buffer_semaphore_submit_info_pre.signal_value =
            use_gpu_culling ? SemaphoreValue::G_BUFFER_LAST_VISIBLE : SemaphoreValue::G_BUFFER_ALL;
    frame_asset.g_buffer_pre.submit({cmd_buffer_pre}, m_queue, {g_buffer_semaphore_submit_info_pre});

    // depth-attachment
    frame_asset.depth_map = frame_asset.g_buffer_pre.depth_attachment();

    if(use_gpu_culling)
    {
        // generate depth-pyramid
        create_depth_pyramid(frame_asset);

        // post-render will perform actual culling
        m_g_renderer_post.draw_indirect_delegate = [this, &frame_asset](Renderer::indirect_draw_bundle_t &params)
        {
            resize_indirect_draw_buffers(params.num_draws, frame_asset.indirect_draw_params_post);
            params.draws_counts_out = frame_asset.indirect_draw_params_post.draws_counts_out;

            // populate gpu-culling params
            vierkant::gpu_cull_params_t gpu_cull_params = {};
            gpu_cull_params.num_draws = params.num_draws;
            gpu_cull_params.camera = frame_asset.cull_result.camera;
            gpu_cull_params.queue = m_queue;
            gpu_cull_params.query_pool = frame_asset.query_pool;
            gpu_cull_params.query_index = SemaphoreValue::CULLING;
            gpu_cull_params.frustum_cull = frame_asset.settings.frustum_culling;
            gpu_cull_params.occlusion_cull = frame_asset.settings.occlusion_culling;
            gpu_cull_params.lod_enabled = frame_asset.settings.enable_lod;
            gpu_cull_params.depth_pyramid = frame_asset.depth_pyramid;
            gpu_cull_params.draws_in = frame_asset.indirect_draw_params_main.draws_in;
            gpu_cull_params.mesh_draws_in = frame_asset.indirect_draw_params_main.mesh_draws;
            gpu_cull_params.mesh_entries_in = frame_asset.indirect_draw_params_main.mesh_entries;

            gpu_cull_params.draws_out_main = frame_asset.indirect_draw_params_main.draws_out;
            gpu_cull_params.draws_counts_out_main = frame_asset.indirect_draw_params_main.draws_counts_out;
            gpu_cull_params.draws_out_post = params.draws_out;
            gpu_cull_params.draws_counts_out_post = frame_asset.indirect_draw_params_post.draws_counts_out;

            gpu_cull_params.semaphore_submit_info.semaphore = frame_asset.timeline.handle();
            gpu_cull_params.semaphore_submit_info.wait_value = SemaphoreValue::DEPTH_PYRAMID;
            gpu_cull_params.semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            gpu_cull_params.semaphore_submit_info.signal_value = SemaphoreValue::CULLING;

            frame_asset.stats.draw_cull_result = vierkant::gpu_cull(frame_asset.gpu_cull_context, gpu_cull_params);
        };

        vierkant::semaphore_submit_info_t g_buffer_semaphore_submit_info = {};
        g_buffer_semaphore_submit_info.semaphore = frame_asset.timeline.handle();
        g_buffer_semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        g_buffer_semaphore_submit_info.wait_value = SemaphoreValue::CULLING;
        g_buffer_semaphore_submit_info.signal_value = SemaphoreValue::G_BUFFER_ALL;

        frame_asset.timings_map[G_BUFFER_ALL] = m_g_renderer_post.last_frame_ms();
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

    size_t index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                   m_g_renderer_main.num_concurrent_frames();
    auto &frame_asset = m_frame_assets[index];

    frame_asset.timings_map[SemaphoreValue::LIGHTING] =
            m_light_renderer.last_frame_ms() + m_sky_renderer.last_frame_ms();

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

    std::vector<lightsource_ubo_t> lights_ubo;//= {vierkant::convert_light(l)};
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

void PBRDeferred::post_fx_pass(vierkant::Renderer &renderer, const CameraPtr &cam, const vierkant::ImagePtr &color,
                               const vierkant::ImagePtr &depth)
{
    size_t frame_index = (m_g_renderer_main.current_index() + m_g_renderer_main.num_concurrent_frames() - 1) %
                         m_g_renderer_main.num_concurrent_frames();
    size_t last_frame_index =
            (frame_index + m_g_renderer_main.num_concurrent_frames() - 1) % m_g_renderer_main.num_concurrent_frames();

    auto &frame_asset = m_frame_assets[frame_index];

    size_t buffer_index = 0;
    vierkant::ImagePtr output_img = color;

    // get next set of pingpong assets, increment index
    auto pingpong_render = [&frame_asset, &buffer_index, queue = m_queue](
            vierkant::drawable_t &drawable,
            const std::vector<vierkant::semaphore_submit_info_t> &semaphore_submit_infos = {})
            -> vierkant::ImagePtr
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
        // assign history
        auto history_color = m_frame_assets[last_frame_index].taa_buffer.color_attachment();
        auto history_depth = m_frame_assets[last_frame_index].g_buffer_post.depth_attachment();

        auto drawable = m_drawable_taa;
        drawable.descriptors[0].images = {output_img, depth,
                                          frame_asset.g_buffer_post.color_attachment(G_BUFFER_MOTION), history_color,
                                          history_depth};

        if(!drawable.descriptors[1].buffers.empty())
        {
            drawable.descriptors[1].buffers.front()->set_data(&frame_asset.camera_params, sizeof(camera_params_t));
        }
        m_taa_renderer.stage_drawable(drawable);

        frame_asset.timings_map[SemaphoreValue::TAA] = m_taa_renderer.last_frame_ms();
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
        if(frame_asset.settings.bloom)
        {
            vierkant::semaphore_submit_info_t bloom_semaphore_info = {};
            bloom_semaphore_info.semaphore = frame_asset.timeline.handle();
            bloom_semaphore_info.signal_value = SemaphoreValue::BLOOM;
            bloom_semaphore_info.signal_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            bloom_img = frame_asset.bloom->apply(output_img, m_queue, {bloom_semaphore_info});
        }

        // motionblur
        auto motion_img = m_empty_img;
        if(frame_asset.settings.motionblur)
        {
            motion_img = frame_asset.g_buffer_post.color_attachment(G_BUFFER_MOTION);
        }

        composition_ubo_t comp_ubo = {};
        comp_ubo.exposure = frame_asset.settings.exposure;
        comp_ubo.gamma = frame_asset.settings.gamma;

        using duration_t = std::chrono::duration<float>;
        comp_ubo.time_delta = duration_t(frame_asset.timestamp - m_frame_assets[last_frame_index].timestamp).count();
        comp_ubo.motionblur_gain = frame_asset.settings.motionblur_gain;

        frame_asset.composition_ubo->set_data(&comp_ubo, sizeof(composition_ubo_t));

        m_drawable_bloom.descriptors[0].images = {output_img, bloom_img, motion_img};
        m_drawable_bloom.descriptors[1].buffers = {frame_asset.composition_ubo};

        vierkant::semaphore_submit_info_t tonemap_semaphore_info = {};
        tonemap_semaphore_info.semaphore = frame_asset.timeline.handle();
        tonemap_semaphore_info.wait_value = frame_asset.settings.bloom ? SemaphoreValue::BLOOM
                                                                       : SemaphoreValue::INVALID;
        tonemap_semaphore_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
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
    size_t last_index = (m_g_renderer_main.num_concurrent_frames() + m_g_renderer_main.current_index() - 1) %
                        m_g_renderer_main.num_concurrent_frames();
    return m_frame_assets[last_index].g_buffer_post;
}

const vierkant::Framebuffer &PBRDeferred::lighting_buffer() const
{
    size_t last_index = (m_light_renderer.num_concurrent_frames() + m_light_renderer.current_index() - 1) %
                        m_light_renderer.num_concurrent_frames();
    return m_frame_assets[last_index].lighting_buffer;
}

void PBRDeferred::set_environment(const ImagePtr &lambert, const ImagePtr &ggx)
{
    m_conv_lambert = lambert;
    m_conv_ggx = ggx;
}

void vierkant::PBRDeferred::resize_storage(vierkant::PBRDeferred::frame_asset_t &asset, const glm::uvec2 &resolution)
{
    glm::uvec2 previous_size = {asset.g_buffer_post.extent().width, asset.g_buffer_post.extent().height};
    asset.settings.resolution = glm::max(resolution, glm::uvec2(16));

    VkExtent3D size = {asset.settings.resolution.x, asset.settings.resolution.y, 1};

    VkViewport viewport = {};
    viewport.width = static_cast<float>(size.width);
    viewport.height = static_cast<float>(size.height);
    viewport.maxDepth = 1;

    m_g_renderer_main.viewport = viewport;
    m_g_renderer_post.viewport = viewport;
    m_light_renderer.viewport = viewport;
    m_sky_renderer.viewport = viewport;
    m_taa_renderer.viewport = viewport;

    // nothing to do
    if(asset.g_buffer_post && asset.g_buffer_post.color_attachment()->extent() == size){ return; }

    m_logger->trace("resizing storage: {} x {} -> {} x {}", previous_size.x, previous_size.y, resolution.x,
                    resolution.y);
    asset.recycle_commands = false;
    vierkant::RenderPassPtr lighting_renderpass, sky_renderpass, post_fx_renderpass;

    // G-buffer (pre and post occlusion-culling)
    asset.g_buffer_pre = create_g_buffer(m_device, size);

    auto renderpass_no_clear_depth =
            vierkant::create_renderpass(m_device, asset.g_buffer_pre.attachments(), false, false);
    asset.g_buffer_post = vierkant::Framebuffer(m_device, asset.g_buffer_pre.attachments(), renderpass_no_clear_depth);
    asset.g_buffer_post.clear_color = {{0.f, 0.f, 0.f, 0.f}};

    // init lighting framebuffer
    vierkant::attachment_map_t lighting_attachments, sky_attachments;
    vierkant::Image::Format img_attachment_16f = {};
    img_attachment_16f.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_attachment_16f.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    img_attachment_16f.extent = size;
    lighting_attachments[vierkant::AttachmentType::Color] = {vierkant::Image::create(m_device, img_attachment_16f)};

    sky_attachments = lighting_attachments;

    // use depth from g_buffer
    sky_attachments[vierkant::AttachmentType::DepthStencil] = {asset.g_buffer_post.depth_attachment()};

    lighting_renderpass = vierkant::create_renderpass(m_device, lighting_attachments, true, false);
    sky_renderpass = vierkant::create_renderpass(m_device, sky_attachments, false, false);
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
    post_fx_buffer_info.renderpass = post_fx_renderpass;

    // create renderer for g-buffer-pass
    vierkant::Renderer::create_info_t post_render_info = {};
    post_render_info.num_frames_in_flight = m_frame_assets.size();
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport = viewport;
    post_render_info.pipeline_cache = m_pipeline_cache;

    // create post_fx ping pong buffers and renderers
    for(auto &post_fx_ping_pong: asset.post_fx_ping_pongs)
    {
        post_fx_ping_pong.framebuffer = vierkant::Framebuffer(m_device, post_fx_buffer_info);
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

void PBRDeferred::create_depth_pyramid(frame_asset_t &frame_asset)
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

    descriptor_t &input_sampler_desc = computable.descriptors[0];
    input_sampler_desc.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    input_sampler_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;

    descriptor_t &output_image_desc = computable.descriptors[1];
    output_image_desc.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    output_image_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;

    descriptor_t &ubo_desc = computable.descriptors[2];
    ubo_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;

    vierkant::Compute::create_info_t compute_info = {};
    compute_info.pipeline_cache = m_pipeline_cache;
    compute_info.command_pool = m_command_pool;

    for(uint32_t i = frame_asset.depth_pyramid_computes.size(); i < frame_asset.depth_pyramid->num_mip_levels(); ++i)
    {
        frame_asset.depth_pyramid_computes.emplace_back(m_device, compute_info);
    }

    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image = frame_asset.depth_pyramid->image();
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;

    frame_asset.depth_pyramid_cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
    frame_asset.depth_pyramid_cmd_buffer.begin();

    // pre depth-pyramid timestamp
    vkCmdWriteTimestamp2(frame_asset.depth_pyramid_cmd_buffer.handle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         frame_asset.query_pool.get(), SemaphoreValue::G_BUFFER_LAST_VISIBLE);

    // transition all mips to general layout for writing
    frame_asset.depth_pyramid->transition_layout(VK_IMAGE_LAYOUT_GENERAL,
                                                 frame_asset.depth_pyramid_cmd_buffer.handle());

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
        vkCmdPipelineBarrier2(frame_asset.depth_pyramid_cmd_buffer.handle(), &dependency_info);
    }

    // depth-pyramid timestamp
    vkCmdWriteTimestamp2(frame_asset.depth_pyramid_cmd_buffer.handle(), VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         frame_asset.query_pool.get(), SemaphoreValue::DEPTH_PYRAMID);

    vierkant::semaphore_submit_info_t pyramid_semaphore_submit_info = {};
    pyramid_semaphore_submit_info.semaphore = frame_asset.timeline.handle();
    pyramid_semaphore_submit_info.wait_value = SemaphoreValue::G_BUFFER_LAST_VISIBLE;
    pyramid_semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    pyramid_semaphore_submit_info.signal_value = SemaphoreValue::DEPTH_PYRAMID;
    frame_asset.depth_pyramid_cmd_buffer.submit(m_queue, false, VK_NULL_HANDLE, {pyramid_semaphore_submit_info});
}

void PBRDeferred::resize_indirect_draw_buffers(uint32_t num_draws, Renderer::indirect_draw_bundle_t &params)
{
    // reserve space for indirect drawing-commands
    const size_t num_bytes = std::max<size_t>(num_draws * sizeof(Renderer::indexed_indirect_command_t), 1ul << 20);

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

void PBRDeferred::update_timing(frame_asset_t &frame_asset)
{
    timings_t &timings_result = frame_asset.stats.timings;
    frame_asset.stats.timestamp = frame_asset.timestamp;

    constexpr size_t query_count = SemaphoreValue::CULLING + 1;

    uint64_t timestamps[query_count] = {};
    auto query_result = vkGetQueryPoolResults(m_device->handle(), frame_asset.query_pool.get(), 0, query_count,
                                              sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

    auto timestamp_period = m_device->properties().limits.timestampPeriod;

    auto millis = [&](SemaphoreValue val) -> double_millisecond_t
    {
        auto frame_ns = std::chrono::nanoseconds(
                static_cast<uint64_t>(double(timestamps[val] - timestamps[val - 1]) * timestamp_period));
        return std::chrono::duration_cast<double_millisecond_t>(frame_ns);
    };

    if(query_result == VK_SUCCESS)
    {
        for(uint32_t i = G_BUFFER_LAST_VISIBLE; i <= SemaphoreValue::CULLING; ++i)
        {
            auto val = SemaphoreValue(i);
            frame_asset.timings_map[val] = millis(val);
        }
    }

    timings_result.g_buffer_pre_ms = frame_asset.timings_map[SemaphoreValue::G_BUFFER_LAST_VISIBLE].count();
    timings_result.depth_pyramid_ms = frame_asset.timings_map[SemaphoreValue::DEPTH_PYRAMID].count();
    timings_result.culling_ms = frame_asset.timings_map[SemaphoreValue::CULLING].count();
    timings_result.g_buffer_post_ms = frame_asset.timings_map[SemaphoreValue::G_BUFFER_ALL].count();
    timings_result.lighting_ms = frame_asset.timings_map[SemaphoreValue::LIGHTING].count();
    timings_result.taa_ms = frame_asset.timings_map[SemaphoreValue::TAA].count();
    timings_result.tonemap_bloom_ms = frame_asset.timings_map[SemaphoreValue::TONEMAP].count();

    timings_result.total_ms = timings_result.g_buffer_pre_ms + timings_result.depth_pyramid_ms +
                              timings_result.culling_ms + timings_result.g_buffer_post_ms + timings_result.lighting_ms +
                              timings_result.taa_ms + timings_result.tonemap_bloom_ms;

    frame_asset.stats.timings = timings_result;

    m_statistics.push_back(frame_asset.stats);
    while(m_statistics.size() > frame_asset.settings.timing_history_size){ m_statistics.pop_front(); }

    // reset query-pool
    vkResetQueryPool(m_device->handle(), frame_asset.query_pool.get(), 0, query_count);
    frame_asset.timings_map.clear();
}

bool operator==(const PBRDeferred::settings_t &lhs, const PBRDeferred::settings_t &rhs)
{
    // TODO: maybe too wonky
    return memcmp(&lhs, &rhs, sizeof(PBRDeferred::settings_t)) == 0;
}

}// namespace vierkant