//
// Created by crocdialer on 3/20/21.
//

#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/shaders.hpp>

namespace vierkant
{

using duration_t = std::chrono::duration<float>;

PBRPathTracerPtr PBRPathTracer::create(const DevicePtr &device, const PBRPathTracer::create_info_t &create_info)
{
    return vierkant::PBRPathTracerPtr(new PBRPathTracer(device, create_info));
}

PBRPathTracer::PBRPathTracer(const DevicePtr &device, const PBRPathTracer::create_info_t &create_info) :
        m_device(device)
{
    // set queue, fallback to first graphics-queue
    m_queue = create_info.queue ? create_info.queue : device->queue();

    m_command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // memorypool with 256MB blocks
    constexpr size_t block_size = 1U << 28U;
    constexpr size_t min_num_blocks = 1, max_num_blocks = 0;
    auto pool = vierkant::Buffer::create_pool(m_device,
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                                              VMA_MEMORY_USAGE_GPU_ONLY,
                                              block_size, min_num_blocks, max_num_blocks,
                                              VMA_POOL_CREATE_BUDDY_ALGORITHM_BIT);

    // create our raytracing-thingies
    vierkant::RayTracer::create_info_t ray_tracer_create_info = {};
    ray_tracer_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    ray_tracer_create_info.pipeline_cache = create_info.pipeline_cache;
    m_ray_tracer = vierkant::RayTracer(device, ray_tracer_create_info);
    m_ray_builder = vierkant::RayBuilder(device, m_queue, pool);
    m_compaction = create_info.compaction;

    // denoise compute
    vierkant::Compute::create_info_t compute_info = {};
    compute_info.num_frames_in_flight = create_info.num_frames_in_flight;
    compute_info.pipeline_cache = create_info.pipeline_cache;
    m_compute = vierkant::Compute(device, compute_info);

    vierkant::Compute::computable_t denoise_computable = {};
    denoise_computable.extent = create_info.size;
    denoise_computable.pipeline_info.shader_stage = vierkant::create_shader_module(m_device,
                                                                                   vierkant::shaders::ray::denoise_comp);

    vierkant::Framebuffer::create_info_t post_fx_buffer_info = {};
    post_fx_buffer_info.size = create_info.size;
    post_fx_buffer_info.color_attachment_format.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // create renderer for post-fx-pass
    vierkant::RenderPassPtr post_fx_renderpass;
    vierkant::Renderer::create_info_t post_render_info = {};
    post_render_info.num_frames_in_flight = create_info.num_frames_in_flight;
    post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    post_render_info.viewport.width = create_info.size.width;
    post_render_info.viewport.height = create_info.size.height;
    post_render_info.viewport.maxDepth = 1;
    post_render_info.pipeline_cache = create_info.pipeline_cache;

    m_frame_assets.resize(create_info.num_frames_in_flight);

    for(auto &frame_asset : m_frame_assets)
    {
        frame_asset.tracable.extent = create_info.size;

        // create a storage image
        vierkant::Image::Format storage_format = {};
        storage_format.extent = create_info.size;
        storage_format.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        storage_format.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        storage_format.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
        frame_asset.storage.color = vierkant::Image::create(m_device, storage_format);
        frame_asset.storage.normals = vierkant::Image::create(m_device, storage_format);
        frame_asset.storage.positions = vierkant::Image::create(m_device, storage_format);

        // create a denoise image
        vierkant::Image::Format denoise_format = {};
        denoise_format.extent = create_info.size;
        denoise_format.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        denoise_format.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        denoise_format.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
        frame_asset.denoise_image = vierkant::Image::create(m_device, denoise_format);
        frame_asset.denoise_computable = denoise_computable;

        // denoise-compute
        vierkant::descriptor_t desc_denoise_input = {};
        desc_denoise_input.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        desc_denoise_input.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_denoise_input.image_samplers = {frame_asset.storage.color, frame_asset.storage.normals,
                                             frame_asset.storage.positions};
        frame_asset.denoise_computable.descriptors[0] = desc_denoise_input;

        vierkant::descriptor_t desc_denoise_output = {};
        desc_denoise_output.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        desc_denoise_output.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_denoise_output.image_samplers = {frame_asset.denoise_image};
        frame_asset.denoise_computable.descriptors[1] = desc_denoise_output;

        // create bloom
        Bloom::create_info_t bloom_info = {};
        bloom_info.size = create_info.size;
//        bloom_info.size.width /= 2;
//        bloom_info.size.height /= 2;
        bloom_info.num_blur_iterations = 3;
        bloom_info.pipeline_cache = create_info.pipeline_cache;
        frame_asset.bloom = Bloom::create(device, bloom_info);

        frame_asset.composition_ubo = vierkant::Buffer::create(device, nullptr, sizeof(composition_ubo_t),
                                                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                               VMA_MEMORY_USAGE_CPU_TO_GPU);

//        // create post_fx ping pong buffers and renderers
//        for(auto &post_fx_ping_pong : frame_asset.post_fx_ping_pongs)
//        {
//            post_fx_ping_pong.framebuffer = vierkant::Framebuffer(device, post_fx_buffer_info, post_fx_renderpass);
//            post_fx_renderpass = post_fx_ping_pong.framebuffer.renderpass();
//            post_fx_ping_pong.framebuffer.clear_color = {{0.f, 0.f, 0.f, 0.f}};
//            post_fx_ping_pong.renderer = vierkant::Renderer(device, post_render_info);
//        }
    }

    auto raygen = vierkant::create_shader_module(m_device, vierkant::shaders::ray::raygen_rgen);
    auto ray_closest_hit = vierkant::create_shader_module(m_device, vierkant::shaders::ray::closesthit_rchit);
    auto ray_miss = vierkant::create_shader_module(m_device, vierkant::shaders::ray::miss_rmiss);
    auto ray_miss_env = vierkant::create_shader_module(m_device, vierkant::shaders::ray::miss_environment_rmiss);

    m_shader_stages = {{VK_SHADER_STAGE_RAYGEN_BIT_KHR,      raygen},
                       {VK_SHADER_STAGE_MISS_BIT_KHR,        ray_miss},
                       {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ray_closest_hit}};

    m_shader_stages_env = {{VK_SHADER_STAGE_RAYGEN_BIT_KHR,      raygen},
                           {VK_SHADER_STAGE_MISS_BIT_KHR,        ray_miss_env},
                           {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ray_closest_hit}};

    // create drawables for post-fx-pass
    {
        m_drawable_raw = {};

        m_drawable_raw.num_vertices = 3;
        m_drawable_raw.use_own_buffers = true;
        m_drawable_raw.pipeline_format.depth_test = false;
        m_drawable_raw.pipeline_format.depth_write = false;

        // same for all fullscreen passes
        m_drawable_raw.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
        m_drawable_raw.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_frag);
        m_drawable_raw.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // descriptor
        m_drawable_raw.descriptors[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        m_drawable_raw.descriptors[0].stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // descriptor
        vierkant::descriptor_t desc_settings_ubo = {};
        desc_settings_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_settings_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // bloom
        m_drawable_bloom = m_drawable_raw;
        m_drawable_bloom.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::bloom_composition_frag);

        // composition ubo
        m_drawable_bloom.descriptors[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        m_drawable_bloom.descriptors[1].stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    m_draw_context = vierkant::DrawContext(m_device);
}

SceneRenderer::render_result_t PBRPathTracer::render_scene(Renderer &renderer,
                                                           const SceneConstPtr &scene,
                                                           const CameraPtr &cam,
                                                           const std::set<std::string> &tags)
{
    auto &frame_asset = m_frame_assets[renderer.current_index()];

    // sync and reset semaphore
    frame_asset.semaphore.wait(SemaphoreValue::RENDER_DONE);
    frame_asset.semaphore = vierkant::Semaphore(m_device);

    // create/update/compact bottom-lvl acceleration-structures
    update_acceleration_structures(frame_asset, scene, tags);

    // pathtracing pass
    path_trace_pass(frame_asset, cam);

    // TODO: denoiser
    denoise_pass(frame_asset);

    // bloom + tonemap
    post_fx_pass(frame_asset);

    // TODO: using the composition drawable yields strange block-artifacts
    renderer.stage_drawable(frame_asset.out_drawable);

    render_result_t ret;
    ret.num_objects = frame_asset.bottom_lvl_assets.size();

    // pass semaphore wait/signal information
    vierkant::semaphore_submit_info_t semaphore_submit_info = {};
    semaphore_submit_info.semaphore = frame_asset.semaphore.handle();
    semaphore_submit_info.wait_value = SemaphoreValue::COMPOSITION;
    semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    semaphore_submit_info.signal_value = SemaphoreValue::RENDER_DONE;
    ret.semaphore_infos = {semaphore_submit_info};
    return ret;
}

void PBRPathTracer::path_trace_pass(frame_assets_t &frame_asset, const CameraPtr &cam)
{
    // push constants
    frame_asset.tracable.push_constants.resize(sizeof(push_constants_t));
    auto &push_constants = *reinterpret_cast<push_constants_t *>(frame_asset.tracable.push_constants.data());
    using namespace std::chrono;
    push_constants.time = duration_cast<duration_t>(steady_clock::now() - m_start_time).count();
    push_constants.batch_index = 0;
    push_constants.disable_material = settings.disable_material;

    frame_asset.cmd_trace = vierkant::CommandBuffer(m_device, m_command_pool.get());
    frame_asset.cmd_trace.begin();

    // update top-level structure
    frame_asset.acceleration_asset = m_ray_builder.create_toplevel(frame_asset.bottom_lvl_assets,
                                                                   frame_asset.cmd_trace.handle());

    update_trace_descriptors(frame_asset, cam);

    // transition storage image
    frame_asset.storage.color->transition_layout(VK_IMAGE_LAYOUT_GENERAL, frame_asset.cmd_trace.handle());

    // run path-tracer
    m_ray_tracer.trace_rays(frame_asset.tracable, frame_asset.cmd_trace.handle());

    frame_asset.cmd_trace.end();

    vierkant::semaphore_submit_info_t semaphore_info = {};
    semaphore_info.semaphore = frame_asset.semaphore.handle();
    semaphore_info.wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    semaphore_info.wait_value = SemaphoreValue::ACCELERATION_UPDATE;
    semaphore_info.signal_value = SemaphoreValue::RAYTRACING;
    frame_asset.cmd_trace.submit(m_queue, false, VK_NULL_HANDLE, {semaphore_info});
}

void PBRPathTracer::denoise_pass(PBRPathTracer::frame_assets_t &frame_asset)
{
    frame_asset.cmd_denoise = vierkant::CommandBuffer(m_device, m_command_pool.get());
    frame_asset.cmd_denoise.begin();

    // transition storage image
    frame_asset.denoise_image->transition_layout(VK_IMAGE_LAYOUT_GENERAL,
                                                 frame_asset.cmd_denoise.handle());

//    // dispatch denoising-kernel
//    m_compute.dispatch(frame_asset.denoise_computable, frame_asset.cmd_denoise.handle());

    frame_asset.denoise_image = frame_asset.storage.color;
    frame_asset.denoise_image->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                 frame_asset.cmd_denoise.handle());

    vierkant::semaphore_submit_info_t semaphore_info = {};
    semaphore_info.semaphore = frame_asset.semaphore.handle();
    semaphore_info.wait_stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    semaphore_info.wait_value = SemaphoreValue::RAYTRACING;
    semaphore_info.signal_value = SemaphoreValue::DENOISER;
    frame_asset.cmd_denoise.submit(m_queue, false, VK_NULL_HANDLE, {semaphore_info});
}

void PBRPathTracer::post_fx_pass(frame_assets_t &frame_asset)
{
    auto output_img = frame_asset.denoise_image;

    semaphore_submit_info_t bloom_submit = {};
    bloom_submit.semaphore = frame_asset.semaphore.handle();
    bloom_submit.wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    bloom_submit.wait_value = SemaphoreValue::DENOISER;
    bloom_submit.signal_value = SemaphoreValue::COMPOSITION;

    // bloom
    if(settings.use_bloom)
    {
        // generate bloom image
        auto bloom_img = frame_asset.bloom->apply(output_img, m_queue, {bloom_submit});

        composition_ubo_t comp_ubo = {};
        comp_ubo.exposure = settings.exposure;
        comp_ubo.gamma = settings.gamma;
        frame_asset.composition_ubo->set_data(&comp_ubo, sizeof(composition_ubo_t));

        frame_asset.out_drawable = m_drawable_bloom;
        frame_asset.out_drawable.descriptors[0].image_samplers = {output_img, bloom_img};
        frame_asset.out_drawable.descriptors[1].buffers = {frame_asset.composition_ubo};
    }
    else
    {
        frame_asset.out_drawable = m_drawable_raw;
        frame_asset.out_drawable.descriptors[0].image_samplers = {output_img};
        vierkant::submit(m_device, m_queue, {}, false, VK_NULL_HANDLE, {bloom_submit});
    }
}

void PBRPathTracer::update_trace_descriptors(frame_assets_t &frame_asset, const CameraPtr &cam)
{
    frame_asset.tracable.descriptors.clear();

    // descriptors
    vierkant::descriptor_t desc_acceleration_structure = {};
    desc_acceleration_structure.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    desc_acceleration_structure.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_acceleration_structure.acceleration_structure = frame_asset.acceleration_asset.structure;
    frame_asset.tracable.descriptors[0] = desc_acceleration_structure;

    vierkant::descriptor_t desc_storage_images = {};
    desc_storage_images.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    desc_storage_images.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_storage_images.image_samplers = {frame_asset.storage.color, frame_asset.storage.normals,
                                          frame_asset.storage.positions};
    frame_asset.tracable.descriptors[1] = desc_storage_images;

    // provide inverse modelview and projection matrices
    std::vector<glm::mat4> matrices = {glm::inverse(cam->view_matrix()),
                                       glm::inverse(cam->projection_matrix())};

    vierkant::descriptor_t desc_matrices = {};
    desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_matrices.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_matrices.buffers = {vierkant::Buffer::create(m_device, matrices,
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU)};
    frame_asset.tracable.descriptors[2] = desc_matrices;

    vierkant::descriptor_t desc_vertex_buffers = {};
    desc_vertex_buffers.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_vertex_buffers.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_vertex_buffers.buffers = frame_asset.acceleration_asset.vertex_buffers;
    desc_vertex_buffers.buffer_offsets = frame_asset.acceleration_asset.vertex_buffer_offsets;
    frame_asset.tracable.descriptors[3] = desc_vertex_buffers;

    vierkant::descriptor_t desc_index_buffers = {};
    desc_index_buffers.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_index_buffers.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_index_buffers.buffers = frame_asset.acceleration_asset.index_buffers;
    desc_index_buffers.buffer_offsets = frame_asset.acceleration_asset.index_buffer_offsets;
    frame_asset.tracable.descriptors[4] = desc_index_buffers;

    vierkant::descriptor_t desc_entries = {};
    desc_entries.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_entries.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_entries.buffers = {frame_asset.acceleration_asset.entry_buffer};
    frame_asset.tracable.descriptors[5] = desc_entries;

    vierkant::descriptor_t desc_materials = {};
    desc_materials.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_materials.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_materials.buffers = {frame_asset.acceleration_asset.material_buffer};
    frame_asset.tracable.descriptors[6] = desc_materials;

    vierkant::descriptor_t desc_textures = {};
    desc_textures.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_textures.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_textures.image_samplers = frame_asset.acceleration_asset.textures;
    frame_asset.tracable.descriptors[7] = desc_textures;

    vierkant::descriptor_t desc_normalmaps = {};
    desc_normalmaps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_normalmaps.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_normalmaps.image_samplers = frame_asset.acceleration_asset.normalmaps;
    frame_asset.tracable.descriptors[8] = desc_normalmaps;

    vierkant::descriptor_t desc_emissions = {};
    desc_emissions.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_emissions.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_emissions.image_samplers = frame_asset.acceleration_asset.emissions;
    frame_asset.tracable.descriptors[9] = desc_emissions;

    vierkant::descriptor_t desc_ao_rough_metal_maps = {};
    desc_ao_rough_metal_maps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_ao_rough_metal_maps.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_ao_rough_metal_maps.image_samplers = frame_asset.acceleration_asset.ao_rough_metal_maps;
    frame_asset.tracable.descriptors[10] = desc_ao_rough_metal_maps;

    if(m_environment)
    {
        vierkant::descriptor_t desc_environment = {};
        desc_environment.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_environment.stage_flags = VK_SHADER_STAGE_MISS_BIT_KHR;
        desc_environment.image_samplers = {m_environment};
        frame_asset.tracable.descriptors[11] = desc_environment;
    }
}

void PBRPathTracer::set_environment(const ImagePtr &cubemap)
{
    m_environment = cubemap;
}

void PBRPathTracer::update_acceleration_structures(PBRPathTracer::frame_assets_t &frame_asset,
                                                   const SceneConstPtr &scene,
                                                   const std::set<std::string> &tags)
{
    // set environment
    m_environment = scene->environment();
    frame_asset.tracable.pipeline_info.shader_stages = m_environment ? m_shader_stages_env : m_shader_stages;

    // TODO: culling, no culling, which volume to use!?
    vierkant::SelectVisitor<vierkant::MeshNode> mesh_selector(tags);
    scene->root()->accept(mesh_selector);

    std::vector<vierkant::semaphore_submit_info_t> semaphore_infos;

    auto previous_builds = std::move(frame_asset.build_results);

    // run compaction on structures from previous frame
    for(auto &[mesh, result] : previous_builds)
    {
        if(m_compaction && result.compacted_assets.empty())
        {
            // run compaction
            m_ray_builder.compact(result);
            m_acceleration_assets[mesh] = result.compacted_assets;

            vierkant::semaphore_submit_info_t wait_info = {};
            wait_info.semaphore = result.semaphore.handle();
            wait_info.wait_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            wait_info.wait_value = RayBuilder::SemaphoreValue::COMPACTED;
            semaphore_infos.push_back(wait_info);

            frame_asset.build_results[mesh] = std::move(result);
        }
    }

    // clear left-overs
    frame_asset.bottom_lvl_assets.clear();

    // schedule non-blocking build+compaction of acceleration structures
    for(const auto &node: mesh_selector.objects)
    {
        auto search_it = m_acceleration_assets.find(node->mesh);

        if(search_it != m_acceleration_assets.end())
        {
            for(auto &asset : search_it->second){ asset->transform = node->global_transform(); }
        }
        else
        {
            // create bottom-lvl
            auto result = m_ray_builder.create_mesh_structures(node->mesh, node->global_transform());
            m_acceleration_assets[node->mesh] = result.acceleration_assets;

            frame_asset.build_results[node->mesh] = std::move(result);
        }
    }

    for(auto &[mesh, result] : frame_asset.build_results)
    {
        vierkant::semaphore_submit_info_t wait_info = {};
        wait_info.semaphore = result.semaphore.handle();
        wait_info.wait_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        wait_info.wait_value = RayBuilder::SemaphoreValue::BUILD;
        semaphore_infos.push_back(wait_info);
    }

    for(auto node: mesh_selector.objects){ frame_asset.bottom_lvl_assets[node->mesh] = m_acceleration_assets[node->mesh]; }

    vierkant::semaphore_submit_info_t signal_info = {};
    signal_info.semaphore = frame_asset.semaphore.handle();
    signal_info.signal_value = SemaphoreValue::ACCELERATION_UPDATE;
    semaphore_infos.push_back(signal_info);

    vierkant::submit(m_device, m_queue, {}, false, VK_NULL_HANDLE, semaphore_infos);
}

}// namespace vierkant

