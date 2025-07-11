#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/shaders.hpp>

namespace vierkant
{

using duration_t = std::chrono::duration<float>;

struct alignas(16) pixel_buffer_t
{
    glm::vec3 radiance;
    float coverage;
    float depth;

    // normal in octahedral encoding
    uint32_t normal;

    uint32_t object_id;
};

PBRPathTracerPtr PBRPathTracer::create(const DevicePtr &device, const PBRPathTracer::create_info_t &create_info)
{
    return vierkant::PBRPathTracerPtr(new PBRPathTracer(device, create_info));
}

PBRPathTracer::PBRPathTracer(const DevicePtr &device, const PBRPathTracer::create_info_t &create_info)
    : m_device(device), m_pipeline_cache(create_info.pipeline_cache), m_random_engine(create_info.seed)
{
    settings = create_info.settings;

    // set queue, fallback to first graphics-queue
    m_queue = create_info.queue ? create_info.queue : device->queue();

    m_command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                           VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // create a DescriptorPool
    vierkant::descriptor_count_t descriptor_counts = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 32},
                                                      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256},
                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256},
                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256}};
    m_descriptor_pool = vierkant::create_descriptor_pool(m_device, descriptor_counts, 128);

    // memorypool
    VmaPoolCreateInfo pool_create_info = {};
    pool_create_info.minAllocationAlignment = 256;

    auto pool = vierkant::Buffer::create_pool(m_device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                                              pool_create_info);

    // create our raytracing-thingies
    vierkant::RayTracer::create_info_t ray_tracer_create_info = {};
    ray_tracer_create_info.num_frames_in_flight = create_info.num_frames_in_flight;
    ray_tracer_create_info.descriptor_pool = m_descriptor_pool;
    ray_tracer_create_info.pipeline_cache = create_info.pipeline_cache;
    m_ray_tracer = vierkant::RayTracer(device, ray_tracer_create_info);
    m_ray_builder = vierkant::RayBuilder(device, m_queue, pool);

    // denoise compute
    vierkant::Compute::create_info_t compute_info = {};
    compute_info.num_frames_in_flight = create_info.num_frames_in_flight;
    compute_info.descriptor_pool = m_descriptor_pool;
    compute_info.pipeline_cache = create_info.pipeline_cache;
    m_compute = vierkant::Compute(device, compute_info);

    VkExtent3D size = {create_info.settings.resolution.x, create_info.settings.resolution.y, 1};

    vierkant::Compute::computable_t denoise_computable = {};
    glm::uvec3 group_count;
    denoise_computable.pipeline_info.shader_stage =
            vierkant::create_shader_module(m_device, vierkant::shaders::ray::denoise_comp, &group_count);
    denoise_computable.extent = size;
    denoise_computable.extent.width = vierkant::group_count(size.width, group_count.x);
    denoise_computable.extent.height = vierkant::group_count(size.height, group_count.y);

    VkPushConstantRange push_constant_range = {};
    push_constant_range.size = sizeof(denoise_params_t);
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    denoise_computable.pipeline_info.push_constant_ranges = {push_constant_range};

    m_frame_contexts.resize(create_info.num_frames_in_flight);

    for(auto &frame_context: m_frame_contexts)
    {
        frame_context.semaphore = vierkant::Semaphore(m_device);

        frame_context.denoise_computable = denoise_computable;

        frame_context.composition_ubo =
                vierkant::Buffer::create(device, nullptr, sizeof(composition_ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame_context.ray_gen_ubo =
                vierkant::Buffer::create(m_device, nullptr, sizeof(camera_params_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame_context.ray_miss_ubo =
                vierkant::Buffer::create(device, &frame_context.settings.environment_factor, sizeof(float),
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        vierkant::CommandBuffer::create_info_t cmd_buffer_info = {};
        cmd_buffer_info.device = m_device;
        cmd_buffer_info.command_pool = m_command_pool.get();

        cmd_buffer_info.name = "cmd_pre_render";
        frame_context.cmd_pre_render = vierkant::CommandBuffer(cmd_buffer_info);

        cmd_buffer_info.name = "cmd_trace";
        frame_context.cmd_trace = vierkant::CommandBuffer(cmd_buffer_info);

        cmd_buffer_info.name = "cmd_denoise";
        frame_context.cmd_denoise = vierkant::CommandBuffer(cmd_buffer_info);

        cmd_buffer_info.name = "cmd_post_fx";
        frame_context.cmd_post_fx = vierkant::CommandBuffer(cmd_buffer_info);

        cmd_buffer_info.name = "cmd_copy_object_id";
        frame_context.cmd_copy_object_id = vierkant::CommandBuffer(cmd_buffer_info);

        frame_context.scene_acceleration_context = m_ray_builder.create_scene_acceleration_context();

        frame_context.query_pool =
                vierkant::create_query_pool(m_device, 2 * SemaphoreValue::MAX_VALUE, VK_QUERY_TYPE_TIMESTAMP);
    }

    auto raygen = vierkant::create_shader_module(m_device, vierkant::shaders::ray::raygen_rgen);
    auto ray_closest_hit = vierkant::create_shader_module(m_device, vierkant::shaders::ray::closesthit_rchit);
    auto ray_any_hit = vierkant::create_shader_module(m_device, vierkant::shaders::ray::anyhit_rahit);
    auto ray_miss = vierkant::create_shader_module(m_device, vierkant::shaders::ray::miss_rmiss);
    auto ray_miss_env = vierkant::create_shader_module(m_device, vierkant::shaders::ray::miss_environment_rmiss);

    m_shader_stages = {{VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygen},
                       {VK_SHADER_STAGE_MISS_BIT_KHR, ray_miss},
                       {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ray_closest_hit},
                       {VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ray_any_hit}};

    m_shader_stages_env = {{VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygen},
                           {VK_SHADER_STAGE_MISS_BIT_KHR, ray_miss_env},
                           {VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, ray_closest_hit},
                           {VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ray_any_hit}};

    // create drawables for post-fx-pass
    {
        m_drawable_tonemap = {};

        m_drawable_tonemap.num_vertices = 3;
        m_drawable_tonemap.use_own_buffers = true;
        m_drawable_tonemap.pipeline_format.depth_test = false;
        m_drawable_tonemap.pipeline_format.depth_write = false;
        m_drawable_tonemap.pipeline_format.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // composition/fullscreen pass
        m_drawable_tonemap.pipeline_format.shader_stages[VK_SHADER_STAGE_VERTEX_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::texture_vert);
        m_drawable_tonemap.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::bloom_composition_frag);

        // descriptors
        m_drawable_tonemap.descriptors[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        m_drawable_tonemap.descriptors[0].stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_drawable_tonemap.descriptors[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        m_drawable_tonemap.descriptors[1].stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    m_draw_context = vierkant::DrawContext(m_device);

    // solid black color
    uint32_t v = 0x00000000;
    vierkant::Image::Format fmt;
    fmt.extent = {1, 1, 1};
    fmt.format = VK_FORMAT_R8G8B8A8_UNORM;
    m_empty_img = vierkant::Image::create(m_device, &v, fmt);
}

SceneRenderer::render_result_t PBRPathTracer::render_scene(Rasterizer &renderer, const SceneConstPtr &scene,
                                                           const CameraPtr &cam, const std::set<std::string> &tags)
{
    auto &frame_context = m_frame_contexts[renderer.current_index()];
    frame_context.statistics.timestamp = std::chrono::steady_clock::now();

    // sync and reset semaphore
    frame_context.semaphore.wait(frame_context.semaphore_value + frame_context.semaphore_value_done);
    frame_context.semaphore_value += frame_context.semaphore_value_done;

    // timing/query-pool, resize storage-assets
    pre_render(frame_context);

    // copy settings for next frame
    frame_context.settings = settings;

    // max num batches reached, bail out
    if(!frame_context.settings.max_num_batches || !frame_context.settings.suspend_trace_when_done ||
       m_batch_index < frame_context.settings.max_num_batches)
    {
        // create/update/compact bottom-lvl acceleration-structures
        update_acceleration_structures(frame_context, scene, tags);

        // pathtracing pass
        path_trace_pass(frame_context, scene, cam);

        // increase batch index
        m_batch_index = std::min<size_t>(m_batch_index + 1, frame_context.settings.max_num_batches);
    }
    else { frame_context.semaphore.signal(frame_context.semaphore_value + SemaphoreValue::RAYTRACING); }

    // image-composition, denoising
    denoise_pass(frame_context);

    // bloom + tonemap
    post_fx_pass(frame_context);

    // stage final output
    m_draw_context.draw_image_fullscreen(renderer, frame_context.out_image, frame_context.out_depth, true,
                                         !frame_context.settings.draw_skybox);

    render_result_t ret;
    ret.object_ids = m_storage.object_ids;
    ret.object_by_index_fn =
            [scene, &scene_asset = frame_context.scene_ray_acceleration](uint32_t draw_id) -> vierkant::id_entry_t {
        auto it = scene_asset.entry_idx_to_object_id.find(draw_id);
        if(it != scene_asset.entry_idx_to_object_id.end()) { return it->second; }
        return {};
    };
    ret.indices_by_id_fn =
            [scene, &scene_asset = frame_context.scene_ray_acceleration](uint32_t object_id) -> std::vector<uint32_t> {
        auto it = scene_asset.object_id_to_entry_indices.find(object_id);
        if(it != scene_asset.object_id_to_entry_indices.end()) { return it->second; }
        return {};
    };

    // pass semaphore wait/signal information
    vierkant::semaphore_submit_info_t semaphore_submit_info = {};
    semaphore_submit_info.semaphore = frame_context.semaphore.handle();
    semaphore_submit_info.wait_value = frame_context.semaphore_value + frame_context.semaphore_value_done;
    semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    ret.semaphore_infos = {semaphore_submit_info};
    return ret;
}

void PBRPathTracer::pre_render(PBRPathTracer::frame_context_t &frame_context)
{
    constexpr size_t query_count = 2 * SemaphoreValue::MAX_VALUE;
    uint64_t timestamps[query_count] = {};
    auto query_result = vkGetQueryPoolResults(m_device->handle(), frame_context.query_pool.get(), 0, query_count,
                                              sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

    double timing_millis[SemaphoreValue::MAX_VALUE] = {};

    auto &timings = frame_context.statistics.timings;
    timings = {};

    if(query_result == VK_SUCCESS || query_result == VK_NOT_READY)
    {
        auto timestamp_period = m_device->properties().core.limits.timestampPeriod;

        for(uint32_t i = 1; i < SemaphoreValue::MAX_VALUE; ++i)
        {
            auto val = SemaphoreValue(i);
            auto measurement = vierkant::timestamp_millis(timestamps, val, timestamp_period);
            timing_millis[val] = measurement;
            timings.total_ms += measurement;
        }
    }

    timings.raybuilder_timings = m_ray_builder.timings(frame_context.scene_acceleration_context);

    timings.raytrace_ms = timing_millis[SemaphoreValue::RAYTRACING];
    timings.bloom_ms = timing_millis[SemaphoreValue::DENOISER];
    timings.tonemap_ms = timing_millis[SemaphoreValue::BLOOM];
    timings.denoise_ms = timing_millis[SemaphoreValue::TONEMAP];

    m_statistics.push_back(frame_context.statistics);
    while(m_statistics.size() > frame_context.settings.timing_history_size) { m_statistics.pop_front(); }

    // reset query-pool
    vkResetQueryPool(m_device->handle(), frame_context.query_pool.get(), 0, query_count);

    frame_context.cmd_pre_render.begin(0);

    // resize storage-assets if necessary
    resize_storage(frame_context, frame_context.settings.resolution);

    frame_context.cmd_pre_render.submit(m_queue);
}

void PBRPathTracer::path_trace_pass(frame_context_t &frame_context, const vierkant::SceneConstPtr & /*scene*/,
                                    const CameraPtr &cam)
{
    // push constants
    frame_context.tracable.push_constants.resize(sizeof(push_constants_t));
    auto &push_constants = *reinterpret_cast<push_constants_t *>(frame_context.tracable.push_constants.data());
    using namespace std::chrono;
    push_constants.time = duration_cast<duration_t>(steady_clock::now() - m_start_time).count();
    push_constants.batch_index = m_batch_index;
    push_constants.num_samples = frame_context.settings.num_samples;
    push_constants.max_trace_depth = frame_context.settings.max_trace_depth;
    push_constants.disable_material = frame_context.settings.disable_material;
    push_constants.draw_skybox = frame_context.settings.draw_skybox;
    push_constants.random_seed = m_random_engine();

    update_trace_descriptors(frame_context, cam);

    frame_context.cmd_trace.begin(0);
    vkCmdWriteTimestamp2(frame_context.cmd_trace.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                         frame_context.query_pool.get(), 2 * SemaphoreValue::RAYTRACING);

    // run path-tracer
    m_ray_tracer.trace_rays(frame_context.tracable, frame_context.cmd_trace.handle());

    vkCmdWriteTimestamp2(frame_context.cmd_trace.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                         frame_context.query_pool.get(), 2 * SemaphoreValue::RAYTRACING + 1);
    frame_context.cmd_trace.end();

    // wait for raybuilder, signal main semaphore
    vierkant::semaphore_submit_info_t semaphore_info = {};
    semaphore_info.semaphore = frame_context.semaphore.handle();
    semaphore_info.signal_value = frame_context.semaphore_value + SemaphoreValue::RAYTRACING;
    frame_context.cmd_trace.submit(m_queue, false, VK_NULL_HANDLE,
                                   {frame_context.scene_ray_acceleration.semaphore_info, semaphore_info});
    frame_context.semaphore_value_done = SemaphoreValue::RAYTRACING;
}

void PBRPathTracer::denoise_pass(PBRPathTracer::frame_context_t &frame_context)
{
    frame_context.cmd_denoise.begin(0);
    vkCmdWriteTimestamp2(frame_context.cmd_denoise.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                         frame_context.query_pool.get(), 2 * SemaphoreValue::DENOISER);

    auto &denoise_params =
            *reinterpret_cast<denoise_params_t *>(frame_context.denoise_computable.push_constants.data());
    denoise_params = {frame_context.settings.resolution, frame_context.settings.denoising};

    // transition storage image
    frame_context.denoise_image->transition_layout(VK_IMAGE_LAYOUT_GENERAL, frame_context.cmd_denoise.handle());

    // write-after-write hazard
    m_storage.object_ids->barrier(m_storage.object_ids->image_layout(), frame_context.cmd_denoise.handle(),
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    m_storage.depth->barrier(frame_context.cmd_denoise.handle(), VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                             VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    // dispatch denoising-kernel
    m_compute.dispatch({frame_context.denoise_computable}, frame_context.cmd_denoise.handle());

    m_storage.depth->barrier(frame_context.cmd_denoise.handle(), VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    frame_context.denoise_image->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                   frame_context.cmd_denoise.handle());

    frame_context.out_depth->copy_from(m_storage.depth, frame_context.cmd_denoise.handle());
    frame_context.out_depth->transition_layout(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, frame_context.cmd_denoise.handle());

    vierkant::semaphore_submit_info_t semaphore_info = {};
    semaphore_info.semaphore = frame_context.semaphore.handle();
    semaphore_info.wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    semaphore_info.wait_value = frame_context.semaphore_value + SemaphoreValue::RAYTRACING;
    semaphore_info.signal_value = frame_context.semaphore_value + SemaphoreValue::DENOISER;

    vkCmdWriteTimestamp2(frame_context.cmd_denoise.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                         frame_context.query_pool.get(), 2 * SemaphoreValue::DENOISER + 1);
    frame_context.cmd_denoise.submit(m_queue, false, VK_NULL_HANDLE, {semaphore_info});
    frame_context.semaphore_value_done = SemaphoreValue::DENOISER;
}

void PBRPathTracer::post_fx_pass(frame_context_t &frame_context)
{
    frame_context.out_image = frame_context.denoise_image;

    // bloom + tonemap
    if(frame_context.settings.tonemap)
    {
        // generate bloom image
        auto bloom_img = m_empty_img;

        // motion shortcut
        auto motion_img = m_empty_img;

        frame_context.cmd_post_fx.begin(0);

        if(frame_context.settings.bloom)
        {
            vkCmdWriteTimestamp2(frame_context.cmd_post_fx.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                 frame_context.query_pool.get(), 2 * SemaphoreValue::BLOOM);
            bloom_img = frame_context.bloom->apply(frame_context.out_image, frame_context.cmd_post_fx.handle());
            vkCmdWriteTimestamp2(frame_context.cmd_post_fx.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                 frame_context.query_pool.get(), 2 * SemaphoreValue::BLOOM + 1);
        }

        composition_ubo_t comp_ubo = {};
        comp_ubo.exposure = frame_context.settings.exposure;
        comp_ubo.gamma = frame_context.settings.gamma;
        frame_context.composition_ubo->set_data(&comp_ubo, sizeof(composition_ubo_t));

        auto drawable = m_drawable_tonemap;
        drawable.descriptors[0].images = {frame_context.out_image, bloom_img, motion_img};
        drawable.descriptors[1].buffers = {frame_context.composition_ubo};

        vierkant::semaphore_submit_info_t tonemap_semaphore_info = {};
        tonemap_semaphore_info.semaphore = frame_context.semaphore.handle();
        tonemap_semaphore_info.wait_value = frame_context.semaphore_value + SemaphoreValue::DENOISER;
        tonemap_semaphore_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        tonemap_semaphore_info.signal_value = frame_context.semaphore_value + SemaphoreValue::TONEMAP;

        frame_context.out_image = frame_context.post_fx_ping_pongs[0].color_attachment();

        vkCmdWriteTimestamp2(frame_context.cmd_post_fx.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             frame_context.query_pool.get(), 2 * SemaphoreValue::TONEMAP);

        vierkant::Framebuffer::begin_rendering_info_t begin_rendering_info = {};
        begin_rendering_info.commandbuffer = frame_context.cmd_post_fx.handle();
        frame_context.post_fx_ping_pongs[0].begin_rendering(begin_rendering_info);

        vierkant::Rasterizer::rendering_info_t rendering_info = {};
        rendering_info.command_buffer = frame_context.cmd_post_fx.handle();
        rendering_info.color_attachment_formats = {frame_context.out_image->format().format};

        frame_context.post_fx_renderer.stage_drawable(drawable);
        frame_context.post_fx_renderer.render(rendering_info);
        vkCmdEndRendering(frame_context.cmd_post_fx.handle());

        vkCmdWriteTimestamp2(frame_context.cmd_post_fx.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             frame_context.query_pool.get(), 2 * SemaphoreValue::TONEMAP + 1);
        frame_context.cmd_post_fx.submit(m_queue, false, VK_NULL_HANDLE, {tonemap_semaphore_info});
        frame_context.semaphore_value_done = SemaphoreValue::TONEMAP;
    }
}

void PBRPathTracer::update_trace_descriptors(frame_context_t &frame_context, const CameraPtr &cam)
{
    frame_context.tracable.descriptors.clear();

    // descriptors
    auto &desc_acceleration_structure = frame_context.tracable.descriptors[0];
    desc_acceleration_structure.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    desc_acceleration_structure.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    desc_acceleration_structure.acceleration_structures = {frame_context.scene_ray_acceleration.top_lvl.structure};

    auto &desc_depth_buffer = frame_context.tracable.descriptors[1];
    desc_depth_buffer.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_depth_buffer.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_depth_buffer.buffers = {m_storage.pixel_buffer};

    vierkant::descriptor_t &desc_matrices = frame_context.tracable.descriptors[2];
    desc_matrices.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_matrices.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_matrices.buffers = {frame_context.ray_gen_ubo};

    vierkant::descriptor_t &desc_vertex_buffers = frame_context.tracable.descriptors[5];
    desc_vertex_buffers.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_vertex_buffers.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    desc_vertex_buffers.buffers = frame_context.scene_ray_acceleration.vertex_buffers;
    desc_vertex_buffers.buffer_offsets = frame_context.scene_ray_acceleration.vertex_buffer_offsets;

    vierkant::descriptor_t &desc_index_buffers = frame_context.tracable.descriptors[6];
    desc_index_buffers.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_index_buffers.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    desc_index_buffers.buffers = frame_context.scene_ray_acceleration.index_buffers;
    desc_index_buffers.buffer_offsets = frame_context.scene_ray_acceleration.index_buffer_offsets;

    vierkant::descriptor_t &desc_entries = frame_context.tracable.descriptors[7];
    desc_entries.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_entries.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    desc_entries.buffers = {frame_context.scene_ray_acceleration.entry_buffer};

    vierkant::descriptor_t &desc_materials = frame_context.tracable.descriptors[8];
    desc_materials.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    desc_materials.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    desc_materials.buffers = {frame_context.scene_ray_acceleration.material_buffer};

    vierkant::descriptor_t &desc_textures = frame_context.tracable.descriptors[9];
    desc_textures.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    desc_textures.stage_flags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    desc_textures.images = frame_context.scene_ray_acceleration.textures;

    // common ubo for miss-shaders
    vierkant::descriptor_t &desc_ray_miss_ubo = frame_context.tracable.descriptors[10];
    desc_ray_miss_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    desc_ray_miss_ubo.stage_flags = VK_SHADER_STAGE_MISS_BIT_KHR;
    desc_ray_miss_ubo.buffers = {frame_context.ray_miss_ubo};

    // assemble camera-ubo
    struct ray_gen_ubo_t
    {
        camera_params_t camera = {};

        // default: air
        media_t camera_media = {};
    };
    ray_gen_ubo_t ray_gen_ubo = {};
    ray_gen_ubo.camera.projection_view = cam->projection_matrix() * mat4_cast(cam->view_transform());
    ray_gen_ubo.camera.projection_inverse = glm::inverse(cam->projection_matrix());
    ray_gen_ubo.camera.view_inverse = vierkant::mat4_cast(cam->global_transform());
    ray_gen_ubo.camera.ortho = true;

    if(auto perspective_cam = std::dynamic_pointer_cast<vierkant::PerspectiveCamera>(cam))
    {
        ray_gen_ubo.camera.ortho = false;
        ray_gen_ubo.camera.fov = perspective_cam->perspective_params.fovy();
        ray_gen_ubo.camera.aperture = frame_context.settings.depth_of_field
                                              ? static_cast<float>(perspective_cam->perspective_params.aperture_size())
                                              : 0.f;
        ray_gen_ubo.camera.focal_distance = perspective_cam->perspective_params.focal_distance;
    }

    // update uniform-buffers
    frame_context.ray_gen_ubo->set_data(&ray_gen_ubo, sizeof(ray_gen_ubo_t));
    frame_context.ray_miss_ubo->set_data(&frame_context.settings.environment_factor, sizeof(float));

    if(m_environment)
    {
        vierkant::descriptor_t &desc_environment = frame_context.tracable.descriptors[11];
        desc_environment.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_environment.stage_flags = VK_SHADER_STAGE_MISS_BIT_KHR;
        desc_environment.images = {m_environment};
    }
}

void PBRPathTracer::update_acceleration_structures(PBRPathTracer::frame_context_t &frame_context,
                                                   const SceneConstPtr &scene, const std::set<std::string> & /*tags*/)
{
    size_t last_index = (m_ray_tracer.current_index() + m_ray_tracer.num_concurrent_frames() - 1) %
                        m_ray_tracer.num_concurrent_frames();
    const auto &last_context = m_frame_contexts[last_index].scene_acceleration_context;

    // set environment
    m_environment = scene->environment();
    bool use_environment = m_environment && frame_context.settings.draw_skybox;
    frame_context.tracable.pipeline_info.shader_stages = use_environment ? m_shader_stages_env : m_shader_stages;

    RayBuilder::build_scene_acceleration_params_t build_scene_params = {};
    build_scene_params.scene = scene;
    build_scene_params.num_micromap_subdivisions = 0;
    build_scene_params.use_compaction = frame_context.settings.compaction;
    build_scene_params.use_scene_assets = true;
    build_scene_params.previous_context = last_context.get();
    frame_context.scene_ray_acceleration =
            m_ray_builder.build_scene_acceleration(frame_context.scene_acceleration_context, build_scene_params);
}

void PBRPathTracer::reset_accumulator() { m_batch_index = 0; }

size_t PBRPathTracer::current_batch() const { return m_batch_index; }

void PBRPathTracer::resize_storage(frame_context_t &frame_context, const glm::uvec2 &resolution)
{
    VkExtent3D size = {resolution.x, resolution.y, 1};

    if(!m_storage.object_ids || m_storage.object_ids->extent() != size)
    {
        // create storage images
        vierkant::Image::Format storage_format = {};
        storage_format.extent = size;
        storage_format.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        storage_format.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        storage_format.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
        storage_format.initial_cmd_buffer = frame_context.cmd_pre_render.handle();

        // shared path-tracer-storage for all frame-assets
        auto object_id_fmt = storage_format;
        object_id_fmt.format = VK_FORMAT_R16_UINT;
        object_id_fmt.name = "PBRPathTracer::m_storage.object_ids";
        m_storage.object_ids = vierkant::Image::create(m_device, object_id_fmt);

        vierkant::Buffer::create_info_t depth_buf_info;
        depth_buf_info.name = "depth storage buffer";
        depth_buf_info.num_bytes = size.width * size.height * sizeof(float);
        depth_buf_info.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR;
        depth_buf_info.device = m_device;
        depth_buf_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
        m_storage.depth = vierkant::Buffer::create(depth_buf_info);

        vierkant::Buffer::create_info_t pix_buf_info;
        pix_buf_info.name = "pixel storage buffer";
        pix_buf_info.num_bytes = size.width * size.height * sizeof(pixel_buffer_t);
        pix_buf_info.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT_KHR;
        pix_buf_info.device = m_device;
        pix_buf_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
        m_storage.pixel_buffer = vierkant::Buffer::create(pix_buf_info);
        m_batch_index = 0;
    }

    if(!frame_context.denoise_image || frame_context.denoise_image->extent() != size)
    {
        glm::uvec2 previous_size = {frame_context.tracable.extent.width, frame_context.tracable.extent.height};
        spdlog::trace("resizing storage: {} x {} -> {} x {}", previous_size.x, previous_size.y, resolution.x,
                      resolution.y);

        frame_context.tracable.extent = size;

        // no recursion required, shadow-rays via ray-queries
        frame_context.tracable.pipeline_info.max_recursion = 1;

        frame_context.denoise_computable.extent = size;
        frame_context.denoise_computable.extent.width = vierkant::group_count(size.width, 16);
        frame_context.denoise_computable.extent.height = vierkant::group_count(size.height, 16);

        // create a denoise image
        vierkant::Image::Format denoise_format = {};
        denoise_format.extent = size;
        denoise_format.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        denoise_format.usage =
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        denoise_format.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
        denoise_format.initial_cmd_buffer = frame_context.cmd_pre_render.handle();
        frame_context.denoise_image = vierkant::Image::create(m_device, denoise_format);

        // denoise-compute
        vierkant::descriptor_t &desc_pixel_in = frame_context.denoise_computable.descriptors[0];
        desc_pixel_in.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_pixel_in.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_pixel_in.buffers = {m_storage.pixel_buffer};

        vierkant::descriptor_t &desc_denoise_output = frame_context.denoise_computable.descriptors[1];
        desc_denoise_output.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        desc_denoise_output.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_denoise_output.images = {frame_context.denoise_image};

        vierkant::descriptor_t &desc_depth_out = frame_context.denoise_computable.descriptors[2];
        desc_depth_out.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        desc_depth_out.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_depth_out.buffers = {m_storage.depth};

        vierkant::descriptor_t &desc_object_id = frame_context.denoise_computable.descriptors[3];
        desc_object_id.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        desc_object_id.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
        desc_object_id.images = {m_storage.object_ids};

        frame_context.denoise_computable.push_constants.resize(sizeof(denoise_params_t));
        *reinterpret_cast<denoise_params_t *>(frame_context.denoise_computable.push_constants.data()) = {
                frame_context.settings.resolution, frame_context.settings.denoising};

        // depth-image
        Image::Format depth_image_format = {};
        depth_image_format.extent = size;
        depth_image_format.format = VK_FORMAT_D32_SFLOAT;
        depth_image_format.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_image_format.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        depth_image_format.initial_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        depth_image_format.name = "depth";
        frame_context.out_depth = {vierkant::Image::create(m_device, depth_image_format)};

        // create bloom
        Bloom::create_info_t bloom_info = {};
        bloom_info.size = {std::max<uint32_t>(size.width / 2, 1), std::max<uint32_t>(size.height / 2, 1), 1};
        bloom_info.num_blur_iterations = 3;
        bloom_info.pipeline_cache = m_pipeline_cache;
        frame_context.bloom = Bloom::create(m_device, bloom_info);

        VkViewport viewport = {};
        viewport.width = static_cast<float>(size.width);
        viewport.height = static_cast<float>(size.height);
        viewport.maxDepth = 1;

        // create renderer for post-fx-passes
        vierkant::Rasterizer::create_info_t post_render_info = {};
        post_render_info.num_frames_in_flight = 1;
        post_render_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
        post_render_info.viewport = viewport;
        post_render_info.pipeline_cache = m_pipeline_cache;
        post_render_info.command_pool = m_command_pool;
        frame_context.post_fx_renderer = vierkant::Rasterizer(m_device, post_render_info);

        vierkant::Framebuffer::create_info_t post_fx_buffer_info = {};
        post_fx_buffer_info.size = size;
        post_fx_buffer_info.color_attachment_format.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        post_fx_buffer_info.color_attachment_format.usage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        // create post_fx ping pong buffers and renderers
        for(auto &post_fx_ping_pong: frame_context.post_fx_ping_pongs)
        {
            post_fx_ping_pong = vierkant::Framebuffer(m_device, post_fx_buffer_info);
            post_fx_ping_pong.clear_color = glm::vec4(0.f);
        }
    }
}

std::vector<uint16_t> PBRPathTracer::pick(const glm::vec2 &normalized_coord, const glm::vec2 &normalized_size)
{
    size_t frame_index = (m_ray_tracer.current_index() + m_ray_tracer.num_concurrent_frames() - 1) %
                         m_ray_tracer.num_concurrent_frames();
    auto &frame_context = m_frame_contexts[frame_index];

    frame_context.cmd_copy_object_id.begin();

    const auto &id_img = m_storage.object_ids;
    auto img_size = glm::vec2(id_img->width(), id_img->height());
    glm::vec2 adjusted_pos = normalized_coord * img_size;
    glm::vec2 adjusted_size = glm::max(normalized_size * img_size, {1, 1});

    // prevent out-of-bounds sampling
    adjusted_pos = glm::clamp(adjusted_pos, glm::vec2(0), img_size - glm::vec2(1));
    adjusted_size = glm::clamp(adjusted_size, glm::vec2(0), img_size - adjusted_pos - glm::vec2(1));

    VkExtent3D img_extent = {static_cast<uint32_t>(adjusted_size.x), static_cast<uint32_t>(adjusted_size.y), 1};
    VkOffset3D img_offset = {static_cast<int32_t>(adjusted_pos.x), static_cast<int32_t>(adjusted_pos.y), 0};

    uint32_t num_object_ids = img_extent.width * img_extent.height;
    uint32_t num_bytes = std::max<uint32_t>(sizeof(uint16_t) * num_object_ids, 512);
    auto buf = vierkant::Buffer::create(m_device, nullptr, num_bytes, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
                                        VMA_MEMORY_USAGE_CPU_ONLY);
    auto prev_layout = id_img->image_layout();
    id_img->copy_to(buf, frame_context.cmd_copy_object_id.handle(), 0, img_offset, img_extent);
    id_img->transition_layout(prev_layout, frame_context.cmd_copy_object_id.handle());

    // wait for frame, copy draw-ids
    vierkant::semaphore_submit_info_t semaphore_info = {};
    semaphore_info.semaphore = frame_context.semaphore.handle();
    semaphore_info.wait_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    semaphore_info.wait_value = frame_context.semaphore_value + frame_context.semaphore_value_done;
    frame_context.cmd_copy_object_id.submit(m_queue, true, VK_NULL_HANDLE, {semaphore_info});

    std::unordered_set<uint16_t> value_set;
    auto ptr = static_cast<const uint16_t *>(buf->map());
    for(uint32_t i = 0; i < num_object_ids; ++i)
    {
        if(ptr[i]) { value_set.insert(std::numeric_limits<uint16_t>::max() - ptr[i]); }
    }
    return {value_set.begin(), value_set.end()};
}

}// namespace vierkant
