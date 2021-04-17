//
// Created by crocdialer on 3/20/21.
//

#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/Visitor.hpp>
#include <vierkant/shaders.hpp>

namespace vierkant
{

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
//    post_render_info.pipeline_cache = m_pipeline_cache;

    m_frame_assets.resize(create_info.num_frames_in_flight);

    for(auto &ray_asset : m_frame_assets)
    {
        ray_asset.tracable.extent = create_info.size;

        // create a storage image
        vierkant::Image::Format img_format = {};
        img_format.extent = create_info.size;
        img_format.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        img_format.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        img_format.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
        ray_asset.storage_image = vierkant::Image::create(m_device, img_format);

        // create bloom
        Bloom::create_info_t bloom_info = {};
        bloom_info.size = create_info.size;
        bloom_info.size.width /= 2;
        bloom_info.size.height /= 2;
        bloom_info.num_blur_iterations = 3;
        ray_asset.bloom = Bloom::create(device, bloom_info);

        ray_asset.composition_ubo = vierkant::Buffer::create(device, nullptr, sizeof(composition_ubo_t),
                                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);

        // create post_fx ping pong buffers and renderers
        for(auto &post_fx_ping_pong : ray_asset.post_fx_ping_pongs)
        {
            post_fx_ping_pong.framebuffer = vierkant::Framebuffer(device, post_fx_buffer_info, post_fx_renderpass);
            post_fx_renderpass = post_fx_ping_pong.framebuffer.renderpass();
            post_fx_ping_pong.framebuffer.clear_color = {{0.f, 0.f, 0.f, 0.f}};
            post_fx_ping_pong.renderer = vierkant::Renderer(device, post_render_info);
        }
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

//    for(auto &ray_asset : m_frame_assets){ ray_asset.tracable.batch_index = 0; }

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

        // descriptor
        vierkant::descriptor_t desc_settings_ubo = {};
        desc_settings_ubo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc_settings_ubo.stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // bloom
        m_composition_drawable = fullscreen_drawable;
        m_composition_drawable.pipeline_format.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_composition_drawable.pipeline_format.shader_stages[VK_SHADER_STAGE_FRAGMENT_BIT] =
                vierkant::create_shader_module(device, vierkant::shaders::fullscreen::bloom_composition_frag);

        // composition ubo
        m_composition_drawable.descriptors[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        m_composition_drawable.descriptors[1].stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    m_draw_context = vierkant::DrawContext(m_device);
}

uint32_t PBRPathTracer::render_scene(Renderer &renderer, const SceneConstPtr &scene, const CameraPtr &cam,
                                     const std::set<std::string> &tags)
{
    // TODO: culling, no culling, which volume to use!?
    vierkant::SelectVisitor<vierkant::MeshNode> mesh_selector(tags);
    scene->root()->accept(mesh_selector);

    // TODO: better strategy for mesh-building, non-blocking
    for(auto node: mesh_selector.objects){ m_ray_builder.add_mesh(node->mesh, node->global_transform()); }

    auto &ray_asset = m_frame_assets[renderer.current_index()];
//    ray_asset.tracable.pipeline_info.shader_stages = m_shader_stages;
    ray_asset.tracable.pipeline_info.shader_stages = scene->environment() ? m_shader_stages_env : m_shader_stages;

    // pathtracing pass
    path_trace_pass(ray_asset, cam);

    // TODO: denoiser

    // bloom + tonemap
    auto out_img = post_fx_pass(ray_asset);

    m_draw_context.draw_image_fullscreen(renderer, out_img);

    uint32_t num_drawables = mesh_selector.objects.size();
    return num_drawables;
}

void PBRPathTracer::path_trace_pass(frame_assets_t &frame_asset, const CameraPtr &cam)
{
    // similar to a fence wait
    frame_asset.semaphore.wait(RENDER_FINISHED);

    frame_asset.semaphore = vierkant::Semaphore(m_device, 0);

    frame_asset.command_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
    frame_asset.command_buffer.begin();

    // keep-alive workaround
    auto tmp = frame_asset.acceleration_asset;

    // update top-level structure
    frame_asset.acceleration_asset = m_ray_builder.create_toplevel(frame_asset.command_buffer.handle());

    update_trace_descriptors(frame_asset, cam);

    // transition storage image
    frame_asset.storage_image->transition_layout(VK_IMAGE_LAYOUT_GENERAL, frame_asset.command_buffer.handle());

    // tada
    m_ray_tracer.trace_rays(frame_asset.tracable, frame_asset.command_buffer.handle());
//    frame_asset.tracable.batch_index++;

    // transition storage image
    frame_asset.storage_image->transition_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                 frame_asset.command_buffer.handle());

    frame_asset.command_buffer.end();

    constexpr uint64_t ray_signal_value = RAYTRACING_FINISHED;
    VkTimelineSemaphoreSubmitInfo timeline_info;
    timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline_info.pNext = nullptr;
    timeline_info.waitSemaphoreValueCount = 0;
    timeline_info.pWaitSemaphoreValues = nullptr;
    timeline_info.signalSemaphoreValueCount = 1;
    timeline_info.pSignalSemaphoreValues = &ray_signal_value;

    auto semaphore_handle = frame_asset.semaphore.handle();
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.pNext = &timeline_info;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &semaphore_handle;
    frame_asset.command_buffer.submit(m_queue, false, VK_NULL_HANDLE, submit_info);
}

vierkant::ImagePtr PBRPathTracer::post_fx_pass(frame_assets_t &frame_asset)
{
    auto output_img = frame_asset.storage_image;

    auto semaphore_handle = frame_asset.semaphore.handle();
    constexpr uint64_t wait_value = RAYTRACING_FINISHED;
    constexpr uint64_t signal_value = RENDER_FINISHED;
    constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    VkTimelineSemaphoreSubmitInfo timeline_wait_info = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    timeline_wait_info.waitSemaphoreValueCount = 1;
    timeline_wait_info.pWaitSemaphoreValues = &wait_value;
    timeline_wait_info.signalSemaphoreValueCount = 0;

    VkSubmitInfo submit_wait_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_wait_info.pNext = &timeline_wait_info;
    submit_wait_info.waitSemaphoreCount = 1;
    submit_wait_info.pWaitSemaphores = &semaphore_handle;
    submit_wait_info.pWaitDstStageMask = &wait_stage;
    submit_wait_info.signalSemaphoreCount = 0;

    VkTimelineSemaphoreSubmitInfo timeline_signal_info = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    timeline_signal_info.waitSemaphoreValueCount = 0;
    timeline_signal_info.signalSemaphoreValueCount = 1;
    timeline_signal_info.pSignalSemaphoreValues = &signal_value;

    VkSubmitInfo submit_signal_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_signal_info.pNext = &timeline_signal_info;
    submit_signal_info.waitSemaphoreCount = 0;
    submit_signal_info.signalSemaphoreCount = 1;
    submit_signal_info.pSignalSemaphores = &semaphore_handle;

    size_t buffer_index = 0;

    // get next set of pingpong assets, increment index
    auto pingpong_render = [queue = m_queue, &submit_signal_info, &frame_asset, &buffer_index](
            Renderer::drawable_t &drawable) -> vierkant::ImagePtr
    {
        auto &pingpong = frame_asset.post_fx_ping_pongs[buffer_index];
        buffer_index = (buffer_index + 1) % frame_asset.post_fx_ping_pongs.size();
        pingpong.renderer.stage_drawable(drawable);
        auto cmd_buf = pingpong.renderer.render(pingpong.framebuffer);
        pingpong.framebuffer.submit({cmd_buf}, queue, submit_signal_info);
        return pingpong.framebuffer.color_attachment(0);
    };

    // bloom
    if(settings.use_bloom)
    {
        // generate bloom image
        auto bloom_img = frame_asset.bloom->apply(output_img, m_queue, submit_wait_info);

        composition_ubo_t comp_ubo = {};
        comp_ubo.exposure = settings.exposure;
        comp_ubo.gamma = settings.gamma;
        frame_asset.composition_ubo->set_data(&comp_ubo, sizeof(composition_ubo_t));

        m_composition_drawable.descriptors[0].image_samplers = {output_img, bloom_img};
        m_composition_drawable.descriptors[1].buffers = {frame_asset.composition_ubo};

        output_img = pingpong_render(m_composition_drawable);
    }

    return output_img;
}

void PBRPathTracer::update_trace_descriptors(frame_assets_t &frame_asset, const CameraPtr &cam)
{
    constexpr size_t max_num_maps = 256;
    frame_asset.acceleration_asset.textures.resize(max_num_maps);
    frame_asset.acceleration_asset.normalmaps.resize(max_num_maps);
    frame_asset.acceleration_asset.emissions.resize(max_num_maps);
    frame_asset.acceleration_asset.ao_rough_metal_maps.resize(max_num_maps);

    // descriptors
    vierkant::descriptor_t desc_acceleration_structure = {};
    desc_acceleration_structure.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    desc_acceleration_structure.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_acceleration_structure.acceleration_structure = frame_asset.acceleration_asset.structure;
    frame_asset.tracable.descriptors[0] = desc_acceleration_structure;

    vierkant::descriptor_t desc_storage_image = {};
    desc_storage_image.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    desc_storage_image.stage_flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    desc_storage_image.image_samplers = {frame_asset.storage_image};
    frame_asset.tracable.descriptors[1] = desc_storage_image;

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
//    else{ frame_asset.tracable.descriptors.erase(11); }

//    if(!frame_asset.tracable.descriptor_set_layout)
//    {
//        frame_asset.tracable.descriptor_set_layout = vierkant::create_descriptor_set_layout(m_device,
//                                                                                            frame_asset.tracable.descriptors);
//    }
//    frame_asset.tracable.pipeline_info.descriptor_set_layouts = {frame_asset.tracable.descriptor_set_layout.get()};
}

void PBRPathTracer::set_environment(const ImagePtr &cubemap)
{
    m_environment = cubemap;
}

}// namespace vierkant

