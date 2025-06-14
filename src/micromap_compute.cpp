#include <vierkant/barycentric_indexing.hpp>
#include <vierkant/micromap_compute.hpp>
#include <vierkant/shaders.hpp>
#include <vierkant/staging_copy.hpp>
#include <vierkant/vertex_splicer.hpp>

namespace vierkant
{

struct mesh_build_data_t
{
    vierkant::BufferPtr micromap;
    vierkant::BufferPtr opacity;
    vierkant::BufferPtr triangles;
    vierkant::BufferPtr indices;
    vierkant::BufferPtr scratch;
};

struct micromap_compute_context_t
{
    // NOTE: the 256-byte alignment is mentioned here:
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBuildMicromapsEXT.html#VUID-vkCmdBuildMicromapsEXT-pInfos-07515
    static constexpr uint32_t data_alignment = 256;

    vierkant::DevicePtr device;
    vierkant::Compute compute;
    glm::uvec3 micromap_compute_local_size{};
    vierkant::Compute::computable_t micromap_computable;

    vierkant::BufferPtr staging_buffer;
    vierkant::BufferPtr params_ubo_buffer;

    // tmp per mesh, during builds
    std::unordered_map<vierkant::MeshConstPtr, mesh_build_data_t> build_data;
    VmaPoolPtr memory_pool;

    uint64_t run_id = 0;
};

struct alignas(16) micromap_params_ubo_t
{
    uint32_t num_triangles;
    uint32_t num_subdivisions;
    uint32_t format;//VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT
    float alpha_cutoff;

    VkDeviceAddress vertex_in;
    VkDeviceAddress index_in;
    VkDeviceAddress micromap_opacity_out;
    VkDeviceAddress micromap_triangle_out;
    VkDeviceAddress micromap_indices_out;
};

micromap_compute_context_handle create_micromap_compute_context(const DevicePtr &device,
                                                                const PipelineCachePtr &pipeline_cache,
                                                                const VmaPoolPtr &memory_pool)
{
    auto ret = micromap_compute_context_handle(new micromap_compute_context_t,
                                               std::default_delete<micromap_compute_context_t>());
    ret->device = device;

    vierkant::Compute::create_info_t compute_create_info = {};
    compute_create_info.pipeline_cache = pipeline_cache;
    ret->compute = vierkant::Compute(device, compute_create_info);

    auto shader_stage = vierkant::create_shader_module(device, vierkant::shaders::ray::micromap_comp,
                                                       &ret->micromap_compute_local_size);
    ret->micromap_computable.pipeline_info.shader_stage = shader_stage;

    vierkant::Buffer::create_info_t staging_buffer_info = {};
    staging_buffer_info.device = device;
    staging_buffer_info.num_bytes = 1U << 20U;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.mem_usage = VMA_MEMORY_USAGE_CPU_ONLY;
    ret->staging_buffer = vierkant::Buffer::create(staging_buffer_info);

    vierkant::Buffer::create_info_t params_buffer_info = {};
    params_buffer_info.device = device;
    params_buffer_info.num_bytes = 1U << 20U;
    params_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    params_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
    ret->params_ubo_buffer = vierkant::Buffer::create(params_buffer_info);

    // memorypool
    if(!memory_pool)
    {
        VmaPoolCreateInfo pool_create_info = {};
        pool_create_info.minAllocationAlignment = micromap_compute_context_t::data_alignment;
        ret->memory_pool = vierkant::Buffer::create_pool(
                device, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY, pool_create_info);
    }
    else { ret->memory_pool = memory_pool; }
    return ret;
}

micromap_compute_result_t micromap_compute(const micromap_compute_context_handle &context,
                                           const micromap_compute_params_t &params)
{
    if(!vkCreateMicromapEXT || params.meshes.empty()) { return {}; }
    const auto num_micro_triangle_bits = static_cast<uint32_t>(params.micromap_format);
    constexpr uint32_t vertex_stride = sizeof(vierkant::packed_vertex_t);

    // scratch-buffers are following same alignment as acceleration-structures
    const uint32_t scratch_alignment =
            context->device->properties().acceleration_structure.minAccelerationStructureScratchOffsetAlignment;

    const uint32_t num_micro_triangles = vierkant::num_micro_triangles(params.num_subdivisions);

    // timestamp start
    if(params.query_pool)
    {
        vkCmdWriteTimestamp2(params.command_buffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, params.query_pool.get(),
                             params.query_index_start);
    }

    micromap_compute_result_t ret;
    ret.run_id = static_cast<micromap_compute_run_id_t>(context->run_id++);

    std::vector<vierkant::Compute::computable_t> computables;
    std::vector<micromap_params_ubo_t> param_ubos;
    std::vector<VkBufferMemoryBarrier2> barriers;

    VkMicromapBuildInfoEXT micromap_build_info_proto = {};
    micromap_build_info_proto.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT;
    micromap_build_info_proto.flags =
            VK_BUILD_MICROMAP_PREFER_FAST_TRACE_BIT_EXT | VK_BUILD_MICROMAP_ALLOW_COMPACTION_BIT_EXT;
    micromap_build_info_proto.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;
    micromap_build_info_proto.usageCountsCount = 1;

    // determine total num_builds
    uint32_t num_builds = 0;
    for(const auto &mesh: params.meshes)
    {
        size_t num_entries = mesh ? mesh->entries.size() : 0;
        num_builds += num_entries;
    }
    std::vector<VkMicromapUsageEXT> micromap_usages(num_builds);
    std::vector<VkMicromapBuildInfoEXT> micromap_build_infos(num_builds, micromap_build_info_proto);
    size_t micromap_build_index = 0;

    for(const auto &mesh: params.meshes)
    {
        size_t num_entries = mesh ? mesh->entries.size() : 0;
        auto &micromap_assets = ret.mesh_micromap_assets[mesh];
        micromap_assets.resize(num_entries);

        struct buffer_sizes_t
        {
            size_t data = 0;
            size_t triangle_data = 0;
            size_t index_data = 0;
            size_t micromap = 0;
            size_t scratch = 0;
        };
        std::vector<buffer_sizes_t> buffer_sizes(num_entries);
        std::vector<buffer_sizes_t> buffer_offsets(num_entries);
        buffer_sizes_t buffer_size_sum = {};

        // iterate to query buffer-sizes for all entries
        for(uint32_t i = 0; i < num_entries; ++i)
        {
            buffer_offsets[i] = buffer_size_sum;
            const auto &entry = mesh->entries[i];
            const auto &lod_0 = entry.lods.front();
            //            const auto &material = mesh->materials[entry.material_index];
            const size_t num_triangles = lod_0.num_indices / 3;

            {
                size_t num_data_bytes = (num_triangles * num_micro_triangles * num_micro_triangle_bits) / 8;
                //                size_t num_data_bytes2 = num_triangles * (1 << (std::max<uint32_t>(3, 2 * params.num_subdivisions) - 3));
                size_t num_triangle_data_bytes = num_triangles * sizeof(VkMicromapTriangleEXT);
                size_t num_index_data_bytes = num_triangles * sizeof(uint32_t);

                buffer_sizes[i].data = num_data_bytes;
                buffer_size_sum.data += aligned_size(num_data_bytes, context->data_alignment);
                buffer_sizes[i].index_data = num_index_data_bytes;
                buffer_size_sum.index_data += aligned_size(num_index_data_bytes, context->data_alignment);
                buffer_sizes[i].triangle_data = num_triangle_data_bytes;
                buffer_size_sum.triangle_data += aligned_size(num_triangle_data_bytes, context->data_alignment);
            }

            // per entry
            VkMicromapUsageEXT micromap_usage;
            micromap_usage.count = num_triangles;
            micromap_usage.subdivisionLevel = params.num_subdivisions;
            micromap_usage.format = params.micromap_format;

            // per entry
            auto micromap_build_info = micromap_build_info_proto;
            micromap_build_info.pUsageCounts = &micromap_usage;

            // query memory requirements
            VkMicromapBuildSizesInfoEXT micromap_size_info = {};
            micromap_size_info.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT;
            vkGetMicromapBuildSizesEXT(context->device->handle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                       &micromap_build_info, &micromap_size_info);

            // store micromap/scratch-sizes
            buffer_sizes[i].micromap = micromap_size_info.micromapSize;
            buffer_size_sum.micromap += aligned_size(micromap_size_info.micromapSize, context->data_alignment);
            buffer_sizes[i].scratch = micromap_size_info.buildScratchSize;
            buffer_size_sum.scratch += aligned_size(micromap_size_info.buildScratchSize, scratch_alignment);
        }// entries

        auto &build_data = context->build_data[mesh];

        // resize buffers
        {
            vierkant::Buffer::create_info_t scratch_buffer_info = {};
            scratch_buffer_info.device = context->device;
            scratch_buffer_info.num_bytes = buffer_size_sum.scratch;
            scratch_buffer_info.alignment = scratch_alignment;
            scratch_buffer_info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            scratch_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            build_data.scratch = vierkant::Buffer::create(scratch_buffer_info);

            // stores input-opacity data
            vierkant::Buffer::create_info_t micromap_input_buffer_format = {};
            micromap_input_buffer_format.device = context->device;
            micromap_input_buffer_format.num_bytes = buffer_size_sum.data;
            micromap_input_buffer_format.alignment = 256;
            micromap_input_buffer_format.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT;
            micromap_input_buffer_format.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            micromap_input_buffer_format.pool = context->memory_pool;
            build_data.opacity = vierkant::Buffer::create(micromap_input_buffer_format);

            // buffer with array of VkMicromapTriangleEXT
            micromap_input_buffer_format.num_bytes = buffer_size_sum.triangle_data;
            build_data.triangles = vierkant::Buffer::create(micromap_input_buffer_format);

            micromap_input_buffer_format.num_bytes = buffer_size_sum.index_data;
            build_data.indices = vierkant::Buffer::create(micromap_input_buffer_format);

            // combined micromap buffer per mesh
            vierkant::Buffer::create_info_t micromap_buffer_info = {};
            micromap_buffer_info.device = context->device;
            micromap_buffer_info.num_bytes = buffer_size_sum.micromap;
            micromap_buffer_info.usage =
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT;
            micromap_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            build_data.micromap = vierkant::Buffer::create(micromap_buffer_info);
        }

        for(uint32_t i = 0; i < num_entries; ++i)
        {
            const auto &entry = mesh->entries[i];
            const auto &lod_0 = entry.lods.front();
            const auto &mesh_material = mesh->materials[entry.material_index];
            const auto &material = mesh_material->m;

            const auto &buffer_size = buffer_sizes[i];
            const auto &buffer_offset = buffer_offsets[i];

            micromap_asset_t micromap_asset = {};
            micromap_asset.num_subdivisions = params.num_subdivisions;
            micromap_asset.micromap_format = params.micromap_format;
            micromap_asset.buffer = build_data.micromap;
            micromap_asset.index_buffer_address = build_data.indices->device_address() + buffer_offset.index_data;
            assert(micromap_asset.index_buffer_address % context->data_alignment == 0);

            // create blank micromap
            VkMicromapCreateInfoEXT micromap_create_info = {};
            micromap_create_info.sType = VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT;
            micromap_create_info.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;
            micromap_create_info.size = buffer_size.micromap;
            micromap_create_info.buffer = build_data.micromap->handle();
            micromap_create_info.offset = buffer_offset.micromap;

            VkMicromapEXT handle;
            vkCheck(vkCreateMicromapEXT(context->device->handle(), &micromap_create_info, nullptr, &handle),
                    "could not create micromap");
            micromap_asset.micromap = {handle, [device = context->device](VkMicromapEXT p) {
                                           vkDestroyMicromapEXT(device->handle(), p, nullptr);
                                       }};

            // schedule compute-dispatch
            micromap_params_ubo_t param_ubo = {};
            param_ubo.num_triangles = lod_0.num_indices / 3;
            param_ubo.num_subdivisions = params.num_subdivisions;
            param_ubo.format = params.micromap_format;
            param_ubo.alpha_cutoff = material.blend_mode == vierkant::BlendMode::Mask ? material.alpha_cutoff : 0.f;
            param_ubo.vertex_in = mesh->vertex_buffer->device_address() + vertex_stride * entry.vertex_offset;
            param_ubo.index_in =
                    mesh->index_buffer->device_address() + vierkant::num_bytes(mesh->index_type) * lod_0.base_index;
            param_ubo.micromap_opacity_out = build_data.opacity->device_address() + buffer_offset.data;
            param_ubo.micromap_triangle_out = build_data.triangles->device_address() + buffer_offset.triangle_data;
            param_ubo.micromap_indices_out = build_data.indices->device_address() + buffer_offset.index_data;

            auto computable = context->micromap_computable;
            computable.extent = {vierkant::group_count(param_ubo.num_triangles, context->micromap_compute_local_size.x),
                                 1, 1};
            auto &descriptor_ubo = computable.descriptors[0];
            descriptor_ubo.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptor_ubo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptor_ubo.buffers = {context->params_ubo_buffer};
            descriptor_ubo.buffer_offsets = {param_ubos.size() * sizeof(micromap_params_ubo_t)};

            auto &descriptor_img = computable.descriptors[1];
            descriptor_img.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptor_img.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_img.images = {mesh_material->textures[vierkant::TextureType::Color]};

            // store params and computable, dispatch later
            param_ubos.push_back(param_ubo);
            computables.push_back(std::move(computable));

            // puzzle together usage/build information
            auto &micromap_usage = micromap_usages[micromap_build_index];
            micromap_usage.count = lod_0.num_indices / 3;
            micromap_usage.subdivisionLevel = params.num_subdivisions;
            micromap_usage.format = params.micromap_format;

            // finalize build-information
            auto &micromap_build_info = micromap_build_infos[micromap_build_index];
            micromap_build_info.usageCountsCount = 1;
            micromap_build_info.pUsageCounts = &micromap_usage;
            micromap_build_info.dstMicromap = micromap_asset.micromap.get();
            micromap_build_info.scratchData.deviceAddress =
                    build_data.scratch->device_address() + buffer_offset.scratch;
            micromap_build_info.data.deviceAddress = build_data.opacity->device_address() + buffer_offset.data;
            micromap_build_info.triangleArray.deviceAddress =
                    build_data.triangles->device_address() + buffer_offset.triangle_data;
            micromap_build_info.triangleArrayStride = sizeof(VkMicromapTriangleEXT);

            assert(micromap_build_info.scratchData.deviceAddress % scratch_alignment == 0);
            assert(micromap_build_info.data.deviceAddress % context->data_alignment == 0);
            assert(micromap_build_info.triangleArray.deviceAddress % context->data_alignment == 0);

            // keep micromap-asset for this entry
            micromap_assets[i] = std::move(micromap_asset);

            // increment index
            micromap_build_index++;
        }// entries
        VkBufferMemoryBarrier2 barrier_opacity = {};
        barrier_opacity.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier_opacity.srcQueueFamilyIndex = barrier_opacity.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_opacity.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
        barrier_opacity.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barrier_opacity.dstAccessMask = VK_ACCESS_2_MICROMAP_READ_BIT_EXT;
        barrier_opacity.dstStageMask = VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT;
        barrier_opacity.buffer = build_data.opacity->handle();
        barrier_opacity.offset = 0;
        barrier_opacity.size = VK_WHOLE_SIZE;

        auto barrier_triangles = barrier_opacity;
        barrier_triangles.buffer = build_data.triangles->handle();

        auto barrier_indices = barrier_opacity;
        barrier_indices.buffer = build_data.indices->handle();

        // store memory-barriers
        barriers.insert(barriers.end(), {barrier_opacity, barrier_indices, barrier_triangles});
    }// meshes

    // staging copies of all params
    vierkant::staging_copy_context_t staging_context = {};
    staging_context.command_buffer = params.command_buffer;
    staging_context.staging_buffer = context->staging_buffer;

    vierkant::staging_copy_info_t staging_info = {};
    staging_info.data = param_ubos.data();
    staging_info.num_bytes = param_ubos.size() * sizeof(micromap_params_ubo_t);
    staging_info.dst_buffer = context->params_ubo_buffer;
    staging_info.dst_access = VK_ACCESS_2_SHADER_READ_BIT;
    staging_info.dst_stage = VK_SHADER_STAGE_COMPUTE_BIT;
    vierkant::staging_copy(staging_context, {staging_info});

    // run compute-pipelines to generate opacity/triangle/index-data for micromap-building
    context->compute.dispatch(computables, params.command_buffer);

    // memory-barriers for all opacity/triangle/index-data
    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dependency_info.pBufferMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(params.command_buffer, &dependency_info);

    // tadaa, build micromaps
    vkCmdBuildMicromapsEXT(params.command_buffer, micromap_build_infos.size(), micromap_build_infos.data());

    barriers.clear();
    for(const auto &mesh: params.meshes)
    {
        auto &build_data = context->build_data[mesh];
        VkBufferMemoryBarrier2 barrier_dst_micromap = {};
        barrier_dst_micromap.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier_dst_micromap.srcQueueFamilyIndex = barrier_dst_micromap.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_dst_micromap.srcAccessMask = VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT;
        barrier_dst_micromap.srcStageMask = VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT;
        barrier_dst_micromap.dstAccessMask = VK_ACCESS_2_MICROMAP_READ_BIT_EXT;
        barrier_dst_micromap.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        barrier_dst_micromap.buffer = build_data.micromap->handle();
        barrier_dst_micromap.offset = 0;
        barrier_dst_micromap.size = VK_WHOLE_SIZE;
        barriers.push_back(barrier_dst_micromap);
    }
    dependency_info.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dependency_info.pBufferMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(params.command_buffer, &dependency_info);

    // timestamp end
    if(params.query_pool)
    {
        vkCmdWriteTimestamp2(params.command_buffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, params.query_pool.get(),
                             params.query_index_end);
    }
    return ret;
}

}// namespace vierkant