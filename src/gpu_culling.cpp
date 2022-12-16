//
// Created by crocdialer on 13.11.22.
//

#include <vierkant/gpu_culling.hpp>
#include "vierkant/shaders.hpp"

namespace vierkant
{

struct alignas(16) draw_cull_data_t
{
    glm::mat4 view = glm::mat4(1);

    float P00, P11, znear, zfar; // symmetric projection parameters
    glm::vec4 frustum; // data for left/right/top/bottom frustum planes

    float lod_base = 15.f;
    float lod_step = 1.5f;

    // depth pyramid size in texels
    glm::vec2 pyramid_size = glm::vec2(0);

    uint32_t num_draws = 0;

    VkBool32 frustum_cull = false;
    VkBool32 occlusion_cull = false;
    VkBool32 distance_cull = false;
    VkBool32 backface_cull = false;
    VkBool32 lod_enabled = false;

    // buffer references
    uint64_t draw_commands_in = 0;
    uint64_t mesh_draws_in = 0;
    uint64_t mesh_entries_in = 0;
    uint64_t draws_out_pre = 0;
    uint64_t draws_out_post = 0;
    uint64_t draw_count_pre = 0;
    uint64_t draw_count_post = 0;
    uint64_t draw_result = 0;
};

draw_cull_result_t gpu_cull(vierkant::gpu_cull_context_t &context,
                            vierkant::CameraConstPtr &camera,
                            const vierkant::ImagePtr &depth_pyramid)
{
    draw_cull_data_t draw_cull_data = {};

    draw_cull_data.num_draws = context.num_draws;
    draw_cull_data.pyramid_size = {depth_pyramid->width(), depth_pyramid->height()};
    draw_cull_data.occlusion_cull = true;
    draw_cull_data.distance_cull = false;
    draw_cull_data.frustum_cull = true;
    draw_cull_data.lod_enabled = true;

    // buffer references
    draw_cull_data.draw_commands_in = context.draws_in->device_address();
    draw_cull_data.mesh_draws_in = context.mesh_draws_in->device_address();
    draw_cull_data.mesh_entries_in = context.mesh_entries_in->device_address();
    draw_cull_data.draws_out_pre = context.draws_out_main->device_address();
    draw_cull_data.draws_out_post = context.draws_out_post->device_address();
    draw_cull_data.draw_count_pre = context.draws_counts_out_main->device_address();
    draw_cull_data.draw_count_post = context.draws_counts_out_post->device_address();
    draw_cull_data.draw_result = context.result_buffer->device_address();

    auto projection = camera->projection_matrix();
    draw_cull_data.P00 = projection[0][0];
    draw_cull_data.P11 = projection[1][1];
    draw_cull_data.znear = camera->near();
    draw_cull_data.zfar = camera->far();
    draw_cull_data.view = camera->view_matrix();

    glm::mat4 projectionT = transpose(projection);
    glm::vec4 frustumX = projectionT[3] + projectionT[0];// x + w < 0
    frustumX /= glm::length(frustumX.xyz());
    glm::vec4 frustumY = projectionT[3] + projectionT[1];// y + w < 0
    frustumY /= glm::length(frustumY.xyz());

    draw_cull_data.frustum = {frustumX.x, frustumX.z, frustumY.y, frustumY.z};
    draw_cull_data.lod_base = 15.f;
    draw_cull_data.lod_step = 1.5f;


    context.computable.extent = {vierkant::group_count(draw_cull_data.num_draws, context.local_size.x), 1, 1};
    draw_cull_data.pyramid_size = {depth_pyramid->width(), depth_pyramid->height()};

    context.draw_cull_buffer->set_data(&draw_cull_data, sizeof(draw_cull_data));

    descriptor_t &depth_pyramid_desc = context.computable.descriptors[0];
    depth_pyramid_desc.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    depth_pyramid_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    depth_pyramid_desc.images = {depth_pyramid};

    descriptor_t &draw_cull_data_desc = context.computable.descriptors[1];
    draw_cull_data_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    draw_cull_data_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    draw_cull_data_desc.buffers = {context.draw_cull_buffer};

    // create / start a new command-buffer
//    frame_asset.cull_cmd_buffer = vierkant::CommandBuffer(m_device, m_command_pool.get());
    context.command_buffer.begin();

    // clear gpu-result-buffer with zeros
    vkCmdFillBuffer(context.command_buffer.handle(), context.result_buffer->handle(), 0, VK_WHOLE_SIZE, 0);

    // clear count-buffers with zeros
    vkCmdFillBuffer(context.command_buffer.handle(), context.draws_counts_out_main->handle(), 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(context.command_buffer.handle(), context.draws_counts_out_post->handle(), 0, VK_WHOLE_SIZE, 0);


    VkBufferMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.buffer = context.draws_out_main->handle();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    std::vector<VkBufferMemoryBarrier2> draw_buffer_barriers(4, barrier);
    draw_buffer_barriers[1].buffer = context.draws_counts_out_main->handle();
    draw_buffer_barriers[2].buffer = context.draws_counts_out_post->handle();
    draw_buffer_barriers[3].buffer = context.draws_out_post->handle();

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.bufferMemoryBarrierCount = draw_buffer_barriers.size();
    dependency_info.pBufferMemoryBarriers = draw_buffer_barriers.data();

    // barrier before writing to indirect-draw-buffer
    vkCmdPipelineBarrier2(context.command_buffer.handle(), &dependency_info);

    // dispatch cull-compute
    context.compute.dispatch({context.computable}, context.command_buffer.handle());

    // swap access
    for(auto &b: draw_buffer_barriers)
    {
        std::swap(b.srcStageMask, b.dstStageMask);
        std::swap(b.srcAccessMask, b.dstAccessMask);
    }

    // memory barrier for draw-indirect buffer
    vkCmdPipelineBarrier2(context.command_buffer.handle(), &dependency_info);

    // memory barrier before copying cull-result buffer
    barrier.buffer = context.result_buffer->handle();
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    dependency_info.bufferMemoryBarrierCount = 1;
    dependency_info.pBufferMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(context.command_buffer.handle(), &dependency_info);

    // copy result into host-visible buffer
    context.result_buffer->copy_to(context.result_buffer_host, context.command_buffer.handle());

    // culling done timestamp
    vkCmdWriteTimestamp2(context.command_buffer.handle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         context.query_pool.get(), context.query_index);

//    vierkant::semaphore_submit_info_t culling_semaphore_submit_info = {};
//    culling_semaphore_submit_info.semaphore = frame_asset.timeline.handle();
//    culling_semaphore_submit_info.wait_value = SemaphoreValue::DEPTH_PYRAMID;
//    culling_semaphore_submit_info.wait_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
//    culling_semaphore_submit_info.signal_value = SemaphoreValue::CULLING;
    context.command_buffer.submit(context.queue, false, VK_NULL_HANDLE, {context.semaphore_submit_info});

    // return results from host-buffer
    return *reinterpret_cast<draw_cull_result_t *>(context.result_buffer_host->map());
}

gpu_cull_context_t create_gpu_cull_context(const DevicePtr &device)
{
    gpu_cull_context_t ret = {};

    ret.command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::COMPUTE,
                                                   VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    ret.draw_cull_buffer = vierkant::Buffer::create(device, nullptr, sizeof(draw_cull_data_t),
                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);


    ret.result_buffer = vierkant::Buffer::create(device, nullptr, sizeof(draw_cull_result_t),
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_MEMORY_USAGE_GPU_ONLY);

    ret.result_buffer_host = vierkant::Buffer::create(
            device, nullptr, sizeof(draw_cull_result_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    *reinterpret_cast<draw_cull_result_t *>(ret.result_buffer_host->map()) = {};

    // indirect-draw cull compute
    auto cull_shader_stage = vierkant::create_shader_module(device, vierkant::shaders::pbr::indirect_cull_comp,
                                                            &ret.local_size);
    ret.computable.pipeline_info.shader_stage = cull_shader_stage;

    vierkant::Compute::create_info_t compute_info = {};
//        compute_info.pipeline_cache = m_pipeline_cache;
    compute_info.command_pool = ret.command_pool;
    ret.compute = vierkant::Compute(device, compute_info);
    return ret;
}

}