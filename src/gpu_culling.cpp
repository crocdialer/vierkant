//
// Created by crocdialer on 13.11.22.
//

#include <vierkant/gpu_culling.hpp>
#include "vierkant/shaders.hpp"

namespace vierkant
{

struct gpu_cull_context_t
{
    vierkant::DevicePtr device;
    vierkant::CommandPoolPtr command_pool;
    vierkant::PipelineCachePtr pipeline_cache;
    vierkant::CommandBuffer cull_cmd_buffer;

    vierkant::Compute cull_compute;
    glm::uvec3 cull_local_size{};
    vierkant::Compute::computable_t cull_computable;

    vierkant::BufferPtr draw_cull_buffer;

    //! draw_cull_result_t buffers
    vierkant::BufferPtr result_buffer;
    vierkant::BufferPtr result_buffer_host;

    //! depth-pyramid assets
    vierkant::CommandBuffer depth_pyramid_cmd_buffer;
    std::vector<vierkant::Compute> depth_pyramid_computes;
    glm::uvec3 depth_pyramid_local_size{};
    vierkant::Compute::computable_t depth_pyramid_computable;
    vierkant::ImagePtr depth_pyramid_img;
};

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

vierkant::ImagePtr create_depth_pyramid(const vierkant::gpu_cull_context_ptr &context,
                                        const create_depth_pyramid_params_t &params)
{
    context->depth_pyramid_cmd_buffer.begin(0);

    auto extent_pyramid_lvl0 = params.depth_map->extent();
    extent_pyramid_lvl0.width = crocore::next_pow_2(1 + extent_pyramid_lvl0.width / 2);
    extent_pyramid_lvl0.height = crocore::next_pow_2(1 + extent_pyramid_lvl0.height / 2);

    // create/resize depth pyramid
    if(!context->depth_pyramid_img || context->depth_pyramid_img->extent() != extent_pyramid_lvl0)
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
        depth_pyramid_fmt.initial_cmd_buffer = context->depth_pyramid_cmd_buffer.handle();
        context->depth_pyramid_img = vierkant::Image::create(context->device, depth_pyramid_fmt);
    }

    std::vector<VkImageView> pyramid_views = {params.depth_map->image_view()};
    std::vector<vierkant::ImagePtr> pyramid_images = {params.depth_map};
    pyramid_images.resize(1 + context->depth_pyramid_img->num_mip_levels(), context->depth_pyramid_img);

    pyramid_views.insert(pyramid_views.end(), context->depth_pyramid_img->mip_image_views().begin(),
                         context->depth_pyramid_img->mip_image_views().end());

    vierkant::Compute::computable_t computable = context->depth_pyramid_computable;

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
    compute_info.pipeline_cache = context->pipeline_cache;
    compute_info.command_pool = context->command_pool;

    for(uint32_t i = context->depth_pyramid_computes.size(); i < context->depth_pyramid_img->num_mip_levels(); ++i)
    {
        context->depth_pyramid_computes.emplace_back(context->device, compute_info);
    }

    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image = context->depth_pyramid_img->image();
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

    // pre depth-pyramid timestamp
    if(params.query_pool)
    {
        vkCmdWriteTimestamp2(context->depth_pyramid_cmd_buffer.handle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             params.query_pool.get(), params.query_index_start);
    }

    // transition all mips to general layout for writing
    context->depth_pyramid_img->transition_layout(VK_IMAGE_LAYOUT_GENERAL,
                                                  context->depth_pyramid_cmd_buffer.handle());

    for(uint32_t lvl = 1; lvl < pyramid_views.size(); ++lvl)
    {
        auto width = std::max(1u, extent_pyramid_lvl0.width >> (lvl - 1));
        auto height = std::max(1u, extent_pyramid_lvl0.height >> (lvl - 1));

        computable.extent = {vierkant::group_count(width, context->depth_pyramid_local_size.x),
                             vierkant::group_count(height, context->depth_pyramid_local_size.y), 1};

        computable.descriptors[0].images = {pyramid_images[lvl - 1]};
        computable.descriptors[0].image_views = {pyramid_views[lvl - 1]};

        computable.descriptors[1].images = {pyramid_images[lvl]};
        computable.descriptors[1].image_views = {pyramid_views[lvl]};

        glm::vec2 image_size = {width, height};
        auto ubo_buffer = vierkant::Buffer::create(context->device, &image_size, sizeof(glm::vec2),
                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        computable.descriptors[2].buffers = {ubo_buffer};

        // dispatch compute shader
        context->depth_pyramid_computes[lvl - 1].dispatch({computable}, context->depth_pyramid_cmd_buffer.handle());

        barrier.subresourceRange.baseMipLevel = lvl - 1;
        vkCmdPipelineBarrier2(context->depth_pyramid_cmd_buffer.handle(), &dependency_info);
    }

    // depth-pyramid timestamp
    vkCmdWriteTimestamp2(context->depth_pyramid_cmd_buffer.handle(), VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                         params.query_pool.get(), params.query_index_end);

    context->depth_pyramid_cmd_buffer.submit(params.queue, false, VK_NULL_HANDLE, {params.semaphore_submit_info});
    return context->depth_pyramid_img;
}

draw_cull_result_t gpu_cull(const vierkant::gpu_cull_context_ptr &context,
                            const gpu_cull_params_t &params)
{
    draw_cull_data_t draw_cull_data = {};

    draw_cull_data.num_draws = params.num_draws;
    draw_cull_data.pyramid_size = {params.depth_pyramid->width(), params.depth_pyramid->height()};
    draw_cull_data.occlusion_cull = params.occlusion_cull;
    draw_cull_data.distance_cull = params.distance_cull;
    draw_cull_data.frustum_cull = params.frustum_cull;
    draw_cull_data.lod_enabled = params.lod_enabled;

    // buffer references
    draw_cull_data.draw_commands_in = params.draws_in->device_address();
    draw_cull_data.mesh_draws_in = params.mesh_draws_in->device_address();
    draw_cull_data.mesh_entries_in = params.mesh_entries_in->device_address();
    draw_cull_data.draws_out_pre = params.draws_out_main->device_address();
    draw_cull_data.draws_out_post = params.draws_out_post->device_address();
    draw_cull_data.draw_count_pre = params.draws_counts_out_main->device_address();
    draw_cull_data.draw_count_post = params.draws_counts_out_post->device_address();
    draw_cull_data.draw_result = context->result_buffer->device_address();

    auto projection = params.camera->projection_matrix();
    draw_cull_data.P00 = projection[0][0];
    draw_cull_data.P11 = projection[1][1];
    draw_cull_data.znear = params.camera->near();
    draw_cull_data.zfar = params.camera->far();
    draw_cull_data.view = params.camera->view_matrix();

    glm::mat4 projectionT = transpose(projection);
    glm::vec4 frustumX = projectionT[3] + projectionT[0];// x + w < 0
    frustumX /= glm::length(frustumX.xyz());
    glm::vec4 frustumY = projectionT[3] + projectionT[1];// y + w < 0
    frustumY /= glm::length(frustumY.xyz());

    draw_cull_data.frustum = {frustumX.x, frustumX.z, frustumY.y, frustumY.z};
    draw_cull_data.lod_base = params.lod_base;
    draw_cull_data.lod_step = params.lod_step;

    context->cull_computable.extent = {vierkant::group_count(params.num_draws, context->cull_local_size.x), 1, 1};
    draw_cull_data.pyramid_size = {params.depth_pyramid->width(), params.depth_pyramid->height()};

    context->draw_cull_buffer->set_data(&draw_cull_data, sizeof(draw_cull_data));

    descriptor_t &depth_pyramid_desc = context->cull_computable.descriptors[0];
    depth_pyramid_desc.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    depth_pyramid_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    depth_pyramid_desc.images = {params.depth_pyramid};

    descriptor_t &draw_cull_data_desc = context->cull_computable.descriptors[1];
    draw_cull_data_desc.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    draw_cull_data_desc.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    draw_cull_data_desc.buffers = {context->draw_cull_buffer};

    // create / start a new command-buffer
    context->cull_cmd_buffer = vierkant::CommandBuffer(context->device, context->command_pool.get());
    context->cull_cmd_buffer.begin();

    // clear gpu-result-buffer with zeros
    vkCmdFillBuffer(context->cull_cmd_buffer.handle(), context->result_buffer->handle(), 0, VK_WHOLE_SIZE, 0);

    // clear count-buffers with zeros
    vkCmdFillBuffer(context->cull_cmd_buffer.handle(), params.draws_counts_out_main->handle(), 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(context->cull_cmd_buffer.handle(), params.draws_counts_out_post->handle(), 0, VK_WHOLE_SIZE, 0);

    VkBufferMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.buffer = params.draws_out_main->handle();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    std::vector<VkBufferMemoryBarrier2> draw_buffer_barriers(4, barrier);
    draw_buffer_barriers[1].buffer = params.draws_counts_out_main->handle();
    draw_buffer_barriers[2].buffer = params.draws_counts_out_post->handle();
    draw_buffer_barriers[3].buffer = params.draws_out_post->handle();

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.bufferMemoryBarrierCount = draw_buffer_barriers.size();
    dependency_info.pBufferMemoryBarriers = draw_buffer_barriers.data();

    // barrier before writing to indirect-draw-buffer
    vkCmdPipelineBarrier2(context->cull_cmd_buffer.handle(), &dependency_info);

    // dispatch cull-compute
    context->cull_compute.dispatch({context->cull_computable}, context->cull_cmd_buffer.handle());

    // swap access
    for(auto &b: draw_buffer_barriers)
    {
        std::swap(b.srcStageMask, b.dstStageMask);
        std::swap(b.srcAccessMask, b.dstAccessMask);
    }

    // memory barrier for draw-indirect buffer
    vkCmdPipelineBarrier2(context->cull_cmd_buffer.handle(), &dependency_info);

    // memory barrier before copying cull-result buffer
    barrier.buffer = context->result_buffer->handle();
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    dependency_info.bufferMemoryBarrierCount = 1;
    dependency_info.pBufferMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(context->cull_cmd_buffer.handle(), &dependency_info);

    // copy result into host-visible buffer
    context->result_buffer->copy_to(context->result_buffer_host, context->cull_cmd_buffer.handle());

    // culling done timestamp
    vkCmdWriteTimestamp2(context->cull_cmd_buffer.handle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                         params.query_pool.get(), params.query_index);

    context->cull_cmd_buffer.submit(params.queue, false, VK_NULL_HANDLE, {params.semaphore_submit_info});

    // return results from host-buffer
    return *reinterpret_cast<draw_cull_result_t *>(context->result_buffer_host->map());
}

gpu_cull_context_ptr create_gpu_cull_context(const DevicePtr &device,
                                             const vierkant::PipelineCachePtr &pipeline_cache)
{
    auto ret = gpu_cull_context_ptr(new gpu_cull_context_t, std::default_delete<gpu_cull_context_t>());
    ret->device = device;
    ret->pipeline_cache = pipeline_cache;
    ret->command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    ret->depth_pyramid_cmd_buffer = vierkant::CommandBuffer(device, ret->command_pool.get());
    ret->draw_cull_buffer = vierkant::Buffer::create(device, nullptr, sizeof(draw_cull_data_t),
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);


    ret->result_buffer = vierkant::Buffer::create(device, nullptr, sizeof(draw_cull_result_t),
                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VMA_MEMORY_USAGE_GPU_ONLY);

    ret->result_buffer_host = vierkant::Buffer::create(
            device, nullptr, sizeof(draw_cull_result_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    *reinterpret_cast<draw_cull_result_t *>(ret->result_buffer_host->map()) = {};

    // indirect-draw cull compute
    auto cull_shader_stage = vierkant::create_shader_module(device, vierkant::shaders::pbr::indirect_cull_comp,
                                                            &ret->cull_local_size);
    ret->cull_computable.pipeline_info.shader_stage = cull_shader_stage;

    vierkant::Compute::create_info_t compute_info = {};
    compute_info.pipeline_cache = ret->pipeline_cache;
    compute_info.command_pool = ret->command_pool;
    ret->cull_compute = vierkant::Compute(device, compute_info);

    // depth pyramid compute
    auto shader_stage = vierkant::create_shader_module(device, vierkant::shaders::pbr::depth_min_reduce_comp,
                                                       &ret->depth_pyramid_local_size);
    ret->depth_pyramid_computable.pipeline_info.shader_stage = shader_stage;

    return ret;
}

}