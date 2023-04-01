//
// Created by crocdialer on 13.02.23.
//

#include <vierkant/mesh_compute.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/staging_copy.hpp>
#include <vierkant/vertex_splicer.hpp>

namespace vierkant
{

struct mesh_compute_context_t
{
    vierkant::DevicePtr device;
    vierkant::CommandPoolPtr command_pool;
    vierkant::PipelineCachePtr pipeline_cache;
    vierkant::CommandBuffer cmd_buffer;

    vierkant::Compute compute;
    glm::uvec3 skin_compute_local_size{}, morph_compute_local_size{};
    vierkant::Compute::computable_t skin_computable, morph_computable;
    vierkant::BufferPtr staging_buffer, skin_param_buffer, bone_buffer, morph_param_buffer;
    vierkant::BufferPtr result_buffer;
    uint64_t run_id = 0;
};

struct alignas(16) skin_compute_params_t
{
    uint32_t num_vertices;
    VkDeviceAddress vertex_in;
    VkDeviceAddress bone_vertex_data_in;
    VkDeviceAddress bones_in;
    VkDeviceAddress vertex_out;
};

struct alignas(16) morph_compute_params_t
{
    VkDeviceAddress vertex_in;
    VkDeviceAddress vertex_out;
    VkDeviceAddress morph_vertex_in;
    uint32_t num_vertices;
    uint32_t morph_count;
    float weights[64];
};

mesh_compute_context_ptr create_mesh_compute_context(const vierkant::DevicePtr &device,
                                                     const vierkant::BufferPtr &result_buffer,
                                                     const vierkant::PipelineCachePtr &pipeline_cache)
{
    auto ret = mesh_compute_context_ptr(new mesh_compute_context_t, std::default_delete<mesh_compute_context_t>());
    ret->device = device;
    ret->pipeline_cache = pipeline_cache;
    ret->command_pool = vierkant::create_command_pool(device, vierkant::Device::Queue::GRAPHICS,
                                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    ret->cmd_buffer = vierkant::CommandBuffer(device, ret->command_pool.get());

    vierkant::Buffer::create_info_t internal_buffer_info = {};
    internal_buffer_info.device = device;
    internal_buffer_info.num_bytes = 1U << 20U;
    internal_buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    internal_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;

    ret->skin_param_buffer = vierkant::Buffer::create(internal_buffer_info);
    ret->bone_buffer = vierkant::Buffer::create(internal_buffer_info);
    ret->morph_param_buffer = vierkant::Buffer::create(internal_buffer_info);

    vierkant::Buffer::create_info_t result_buffer_info = {};
    result_buffer_info.device = device;
    result_buffer_info.num_bytes = 1U << 20U;
    result_buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    result_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;

    ret->result_buffer = result_buffer ? result_buffer : vierkant::Buffer::create(result_buffer_info);

    vierkant::Buffer::create_info_t staging_buffer_info = {};
    staging_buffer_info.device = device;
    staging_buffer_info.num_bytes = 1U << 20U;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_ONLY;
    ret->staging_buffer = vierkant::Buffer::create(staging_buffer_info);

    // skin compute
    auto skin_shader_stage = vierkant::create_shader_module(device, vierkant::shaders::pbr::mesh_skin_comp,
                                                            &ret->skin_compute_local_size);
    ret->skin_computable.pipeline_info.shader_stage = skin_shader_stage;

    // morph compute
    auto morph_shader_stage = vierkant::create_shader_module(device, vierkant::shaders::pbr::mesh_morph_comp,
                                                             &ret->morph_compute_local_size);
    ret->morph_computable.pipeline_info.shader_stage = morph_shader_stage;

    vierkant::Compute::create_info_t compute_info = {};
    compute_info.pipeline_cache = ret->pipeline_cache;
    compute_info.command_pool = ret->command_pool;
    ret->compute = vierkant::Compute(device, compute_info);
    ret->run_id = 0;
    return ret;
}

mesh_compute_result_t mesh_compute(const mesh_compute_context_ptr &context, const mesh_compute_params_t &params)
{
    assert(context);
    assert(params.queue);

    mesh_compute_result_t ret = {};
    ret.result_buffer = context->result_buffer;
    ret.run_id = mesh_compute_run_id_t(context->run_id++);

    // min alignment for storage-buffers
    auto min_alignment = context->device->properties().limits.minStorageBufferOffsetAlignment;

    // determine total sizes, grow buffers if necessary
    {
        uint32_t num_vertex_bytes = 0, num_bone_bytes = 0;
        for(const auto &[id, item]: params.mesh_compute_items)
        {
            const auto &[mesh, animation_state] = item;
            auto num_mesh_bytes = mesh->vertex_buffer->num_bytes();
            num_vertex_bytes += num_mesh_bytes + min_alignment - (num_mesh_bytes % min_alignment);
            auto num_mesh_bone_bytes =
                    vierkant::nodes::num_nodes_in_hierarchy(mesh->root_bone) * sizeof(vierkant::transform_t);
            num_bone_bytes += num_mesh_bone_bytes + min_alignment - (num_mesh_bone_bytes % min_alignment);
        }

        context->result_buffer->set_data(nullptr, num_vertex_bytes);
        context->bone_buffer->set_data(nullptr, num_bone_bytes);
    }

    std::vector<vierkant::transform_t> combined_bone_data;
    std::vector<skin_compute_params_t> combined_skin_params;
    std::vector<morph_compute_params_t> combined_morph_params;
    std::vector<Compute::computable_t> computables;

    VkDeviceSize vertex_offset = 0;

    std::unordered_map<vierkant::animated_mesh_t, VkDeviceSize> cached_offsets;

    for(const auto &[id, item]: params.mesh_compute_items)
    {
        const auto &[mesh, animation_state] = item;

        // avoid computing duplicates
        auto cache_it = cached_offsets.find(item);
        if(cache_it != cached_offsets.end())
        {
            ret.vertex_buffer_offsets[id] = cache_it->second;
            continue;
        }

        uint32_t vertex_stride = mesh->vertex_attribs.begin()->second.stride;
        assert(vertex_stride == sizeof(packed_vertex_t));

        bool animation_update =
                mesh && animation_state.index < mesh->node_animations.size() && (mesh->root_bone || mesh->morph_buffer);

        if(animation_update)
        {
            const auto &animation = mesh->node_animations[animation_state.index];

            // store current offset for this id
            ret.vertex_buffer_offsets[id] = vertex_offset;
            cached_offsets[item] = vertex_offset;

            if(mesh->root_bone)
            {
                // create array of bone-transformations for this mesh+animation-state
                std::vector<vierkant::transform_t> bone_transforms;
                vierkant::nodes::build_node_matrices_bfs(
                        mesh->root_bone, animation, static_cast<float>(animation_state.current_time), bone_transforms);

                // keep track of offsets
                size_t bone_offset = combined_bone_data.size() * sizeof(vierkant::transform_t);
                uint64_t skin_param_buffer_offset = combined_skin_params.size() * sizeof(skin_compute_params_t);
                combined_bone_data.insert(combined_bone_data.end(), bone_transforms.begin(), bone_transforms.end());

                uint32_t num_mesh_vertices = 0;
                for(const auto &entry: mesh->entries) { num_mesh_vertices += entry.num_vertices; }
                assert(mesh->vertex_buffer->num_bytes() == num_mesh_vertices * vertex_stride);

                skin_compute_params_t skin_compute_param = {};
                skin_compute_param.num_vertices = num_mesh_vertices;
                skin_compute_param.vertex_in = mesh->vertex_buffer->device_address();
                skin_compute_param.bone_vertex_data_in = mesh->bone_vertex_buffer->device_address();
                skin_compute_param.bones_in = context->bone_buffer->device_address() + bone_offset;
                skin_compute_param.vertex_out = ret.result_buffer->device_address() + vertex_offset;
                combined_skin_params.push_back(skin_compute_param);

                auto computable = context->skin_computable;
                computable.extent = {vierkant::group_count(num_mesh_vertices, context->skin_compute_local_size.x), 1,
                                     1};

                auto &desc_params = computable.descriptors[0];
                desc_params.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                desc_params.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
                desc_params.buffers = {context->skin_param_buffer};
                desc_params.buffer_offsets = {skin_param_buffer_offset};
                computables.push_back(std::move(computable));

                // advance offset, respect alignment
                vertex_offset += num_mesh_vertices * vertex_stride;
                vertex_offset += vertex_offset % min_alignment;
            }
            else if(mesh->morph_buffer)
            {
                constexpr size_t morph_vertex_stride = sizeof(vierkant::vertex_t);

                // morph-target weights
                std::vector<std::vector<float>> node_morph_weights;
                vierkant::nodes::build_morph_weights_bfs(mesh->root_node, animation,
                                                         static_cast<float>(animation_state.current_time),
                                                         node_morph_weights);

                for(uint32_t i = 0; i < mesh->entries.size(); ++i)
                {
                    const auto &entry = mesh->entries[i];
                    const auto &weights = node_morph_weights[entry.node_index];

                    morph_compute_params_t p = {};
                    p.vertex_in = mesh->vertex_buffer->device_address() + entry.vertex_offset * vertex_stride;
                    p.morph_vertex_in =
                            mesh->morph_buffer->device_address() + entry.morph_vertex_offset * morph_vertex_stride;
                    p.vertex_out = ret.result_buffer->device_address() + vertex_offset;
                    p.num_vertices = entry.num_vertices;
                    p.morph_count = weights.size();

                    assert(p.morph_count * sizeof(float) <= sizeof(p.weights));
                    memcpy(p.weights, weights.data(), weights.size() * sizeof(float));

                    uint64_t morph_param_buffer_offset = combined_morph_params.size() * sizeof(morph_compute_params_t);
                    combined_morph_params.push_back(p);

                    auto computable = context->morph_computable;
                    computable.extent = {vierkant::group_count(entry.num_vertices, context->morph_compute_local_size.x),
                                         1, 1};

                    auto &desc_params = computable.descriptors[0];
                    desc_params.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    desc_params.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
                    desc_params.buffers = {context->morph_param_buffer};
                    desc_params.buffer_offsets = {morph_param_buffer_offset};
                    computables.push_back(std::move(computable));

                    // advance offset, respect alignment
                    vertex_offset += entry.num_vertices * vertex_stride;
                    vertex_offset += vertex_offset % min_alignment;
                }
            }
        }
    }

    context->cmd_buffer.begin(0);
    vkCmdWriteTimestamp2(context->cmd_buffer.handle(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                         params.query_pool.get(), params.query_index_start);

    // staging copies of bones + params
    vierkant::staging_copy_context_t staging_context = {};
    staging_context.command_buffer = context->cmd_buffer.handle();
    staging_context.staging_buffer = context->staging_buffer;

    std::vector<vierkant::staging_copy_info_t> staging_infos(3);

    // bones
    staging_infos[0].data = combined_bone_data.data();
    staging_infos[0].num_bytes = combined_bone_data.size() * sizeof(vierkant::transform_t);
    staging_infos[0].dst_buffer = context->bone_buffer;
    staging_infos[0].dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    staging_infos[0].dst_access = VK_ACCESS_2_SHADER_READ_BIT;

    // skin params
    staging_infos[1].data = combined_skin_params.data();
    staging_infos[1].num_bytes = combined_skin_params.size() * sizeof(skin_compute_params_t);
    staging_infos[1].dst_buffer = context->skin_param_buffer;
    staging_infos[1].dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    staging_infos[1].dst_access = VK_ACCESS_2_SHADER_READ_BIT;

    // morph params
    staging_infos[2].data = combined_morph_params.data();
    staging_infos[2].num_bytes = combined_morph_params.size() * sizeof(morph_compute_params_t);
    staging_infos[2].dst_buffer = context->morph_param_buffer;
    staging_infos[2].dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    staging_infos[2].dst_access = VK_ACCESS_2_SHADER_READ_BIT;

    // issue staging-copy
    vierkant::staging_copy(staging_context, staging_infos);

    // run compute-pipelines
    context->compute.dispatch(computables, context->cmd_buffer.handle());

    // memory read-barrier
    VkBufferMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT |
                           VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    barrier.buffer = ret.result_buffer->handle();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.bufferMemoryBarrierCount = 1;
    dependency_info.pBufferMemoryBarriers = &barrier;

    // barrier reading any vertex-data
    vkCmdPipelineBarrier2(context->cmd_buffer.handle(), &dependency_info);

    if(params.query_pool)
    {
        vkCmdWriteTimestamp2(context->cmd_buffer.handle(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             params.query_pool.get(), params.query_index_end);
    }
    context->cmd_buffer.submit(params.queue, false, VK_NULL_HANDLE, {params.semaphore_submit_info});
    return ret;
}

}// namespace vierkant