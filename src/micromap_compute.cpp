#include <vierkant/micromap_compute.hpp>

namespace vierkant
{

struct mesh_build_data_t
{
    vierkant::BufferPtr omm_input;  // data + triangles + indices, 256-byte aligned sections
    vierkant::BufferPtr micromap;
    vierkant::BufferPtr scratch;
};

struct micromap_compute_context_t
{
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBuildMicromapsEXT.html#VUID-vkCmdBuildMicromapsEXT-pInfos-07515
    static constexpr uint32_t data_alignment = 256;

    vierkant::DevicePtr device;
    std::unordered_map<vierkant::MeshConstPtr, mesh_build_data_t> build_data;
    VmaPoolPtr memory_pool;
    uint64_t run_id = 0;
};

micromap_compute_context_handle create_micromap_compute_context(const DevicePtr &device,
                                                                const PipelineCachePtr & /*pipeline_cache*/,
                                                                const VmaPoolPtr &memory_pool)
{
    auto ret = micromap_compute_context_handle(new micromap_compute_context_t,
                                               std::default_delete<micromap_compute_context_t>());
    ret->device = device;

    if(!memory_pool)
    {
        VmaPoolCreateInfo pool_create_info = {};
        pool_create_info.minAllocationAlignment = micromap_compute_context_t::data_alignment;
        ret->memory_pool = vierkant::Buffer::create_pool(
                device, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT,
                VMA_MEMORY_USAGE_GPU_ONLY, pool_create_info);
    }
    else { ret->memory_pool = memory_pool; }
    return ret;
}

micromap_compute_result_t micromap_compute(const micromap_compute_context_handle &context,
                                           const micromap_compute_params_t &params)
{
    if(!vkCreateMicromapEXT || params.meshes.empty() || !params.omm_cache) { return {}; }

    const uint32_t scratch_alignment =
            context->device->properties().acceleration_structure.minAccelerationStructureScratchOffsetAlignment;
    constexpr uint32_t data_alignment = micromap_compute_context_t::data_alignment;

    if(params.query_pool)
    {
        vkCmdWriteTimestamp2(params.command_buffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, params.query_pool.get(),
                             params.query_index_start);
    }

    micromap_compute_result_t ret;
    ret.run_id = static_cast<micromap_compute_run_id_t>(context->run_id++);

    VkMicromapBuildInfoEXT build_info_proto = {};
    build_info_proto.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT;
    build_info_proto.flags =
            VK_BUILD_MICROMAP_PREFER_FAST_TRACE_BIT_EXT | VK_BUILD_MICROMAP_ALLOW_COMPACTION_BIT_EXT;
    build_info_proto.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;

    struct entry_plan_t
    {
        const vierkant::model::mesh_omm_entry_t *cache_entry = nullptr;
        std::vector<VkMicromapUsageEXT> usages;
        uint32_t entry_idx = 0;
        vierkant::MeshConstPtr mesh;
        size_t data_offset = 0;
        size_t triangles_offset = 0;
        size_t indices_offset = 0;
        size_t micromap_offset = 0;
        size_t scratch_offset = 0;
        VkDeviceSize micromap_size = 0;
        VkDeviceSize scratch_size = 0;
    };

    // group plans by mesh so we can allocate per-mesh buffers
    struct mesh_plan_t
    {
        std::vector<entry_plan_t> entries;
        size_t total_input_size = 0;
        size_t total_micromap_size = 0;
        size_t total_scratch_size = 0;
    };
    std::unordered_map<vierkant::MeshConstPtr, mesh_plan_t> mesh_plans;

    // pass 1: collect plans and query micromap build sizes
    for(const auto &mesh: params.meshes)
    {
        if(!mesh) { continue; }
        auto &mp = mesh_plans[mesh];
        const size_t num_entries = mesh->entries.size();

        for(uint32_t i = 0; i < num_entries; ++i)
        {
            const auto &entry = mesh->entries[i];
            if(entry.material_index >= mesh->material_ids.size()) { continue; }

            // resolve the Color-texture id for this entry to key into the cache
            if(!params.color_texture_lookup) { continue; }
            const vierkant::TextureId color_texture_id = params.color_texture_lookup(mesh, i);
            if(!color_texture_id) { continue; }

            vierkant::model::mesh_omm_key_t key = {mesh->id, i, color_texture_id};
            auto cache_it = params.omm_cache->find(key);
            if(cache_it == params.omm_cache->end()) { continue; }

            entry_plan_t plan;
            plan.cache_entry = &cache_it->second;
            plan.entry_idx = i;
            plan.mesh = mesh;

            // build VkMicromapUsageEXT by counting (subdivisionLevel, format) pairs in triangles[]
            std::unordered_map<uint32_t, uint32_t> lf_counts;
            for(const auto &tri: cache_it->second.triangles)
            {
                uint32_t lf = (uint32_t(tri.subdivisionLevel) << 16) | tri.format;
                lf_counts[lf]++;
            }
            for(const auto &[lf, count]: lf_counts)
            {
                VkMicromapUsageEXT usage = {};
                usage.count = count;
                usage.subdivisionLevel = lf >> 16;
                usage.format = lf & 0xFFFF;
                plan.usages.push_back(usage);
            }

            // query micromap build sizes
            auto query_info = build_info_proto;
            query_info.usageCountsCount = static_cast<uint32_t>(plan.usages.size());
            query_info.pUsageCounts = plan.usages.data();

            VkMicromapBuildSizesInfoEXT size_info = {};
            size_info.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT;
            vkGetMicromapBuildSizesEXT(context->device->handle(),
                                       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &query_info, &size_info);
            plan.micromap_size = size_info.micromapSize;
            plan.scratch_size = size_info.buildScratchSize;

            // record offsets within per-mesh buffers
            plan.data_offset = mp.total_input_size;
            mp.total_input_size +=
                    aligned_size(static_cast<uint32_t>(cache_it->second.data.size()), data_alignment);
            plan.triangles_offset = mp.total_input_size;
            mp.total_input_size += aligned_size(
                    static_cast<uint32_t>(cache_it->second.triangles.size() * sizeof(VkMicromapTriangleEXT)),
                    data_alignment);
            plan.indices_offset = mp.total_input_size;
            mp.total_input_size += aligned_size(
                    static_cast<uint32_t>(cache_it->second.indices.size() * sizeof(int32_t)), data_alignment);

            plan.micromap_offset = mp.total_micromap_size;
            mp.total_micromap_size +=
                    aligned_size(static_cast<uint32_t>(plan.micromap_size), data_alignment);
            plan.scratch_offset = mp.total_scratch_size;
            mp.total_scratch_size +=
                    aligned_size(static_cast<uint32_t>(plan.scratch_size), scratch_alignment);

            mp.entries.push_back(std::move(plan));
        }
    }

    // pass 2: allocate buffers, upload, create micromaps, fill build infos
    // build_infos references plan.usages.data() — plans must not be modified after this point
    std::vector<VkMicromapBuildInfoEXT> all_build_infos;
    std::vector<VkBufferMemoryBarrier2> barriers;

    for(const auto &mesh: params.meshes)
    {
        auto plan_it = mesh_plans.find(mesh);
        if(plan_it == mesh_plans.end() || plan_it->second.entries.empty()) { continue; }

        auto &mp = plan_it->second;
        const size_t num_entries = mesh->entries.size();
        auto &micromap_assets = ret.mesh_micromap_assets[mesh];
        micromap_assets.resize(num_entries);

        auto &build_data = context->build_data[mesh];

        // omm_input: CPU_TO_GPU so we can write directly without a staging command
        {
            vierkant::Buffer::create_info_t info = {};
            info.device = context->device;
            info.num_bytes = mp.total_input_size;
            info.alignment = data_alignment;
            info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                         VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT;
            info.mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            build_data.omm_input = vierkant::Buffer::create(info);
        }
        {
            vierkant::Buffer::create_info_t info = {};
            info.device = context->device;
            info.num_bytes = mp.total_micromap_size;
            info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT;
            info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            info.pool = context->memory_pool;
            build_data.micromap = vierkant::Buffer::create(info);
        }
        {
            vierkant::Buffer::create_info_t info = {};
            info.device = context->device;
            info.num_bytes = mp.total_scratch_size;
            info.alignment = scratch_alignment;
            info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            build_data.scratch = vierkant::Buffer::create(info);
        }

        auto *input_ptr = static_cast<uint8_t *>(build_data.omm_input->map());

        for(auto &plan: mp.entries)
        {
            const auto &ce = *plan.cache_entry;

            memcpy(input_ptr + plan.data_offset, ce.data.data(), ce.data.size());
            memcpy(input_ptr + plan.triangles_offset, ce.triangles.data(),
                   ce.triangles.size() * sizeof(VkMicromapTriangleEXT));
            memcpy(input_ptr + plan.indices_offset, ce.indices.data(),
                   ce.indices.size() * sizeof(int32_t));

            micromap_asset_t asset = {};
            asset.buffer = build_data.micromap;
            asset.index_buffer_address = build_data.omm_input->device_address() + plan.indices_offset;
            asset.micromap_usages = plan.usages;  // copy; plan.usages stays alive for pUsageCounts below

            VkMicromapCreateInfoEXT create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT;
            create_info.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;
            create_info.size = plan.micromap_size;
            create_info.buffer = build_data.micromap->handle();
            create_info.offset = plan.micromap_offset;

            VkMicromapEXT handle;
            vkCheck(vkCreateMicromapEXT(context->device->handle(), &create_info, nullptr, &handle),
                    "could not create micromap");
            asset.micromap = {handle, [device = context->device](VkMicromapEXT p) {
                                  vkDestroyMicromapEXT(device->handle(), p, nullptr);
                              }};

            auto &build_info = all_build_infos.emplace_back(build_info_proto);
            build_info.usageCountsCount = static_cast<uint32_t>(plan.usages.size());
            build_info.pUsageCounts = plan.usages.data();  // stable: plan lives in mesh_plans
            build_info.dstMicromap = asset.micromap.get();
            build_info.data.deviceAddress = build_data.omm_input->device_address() + plan.data_offset;
            build_info.triangleArray.deviceAddress =
                    build_data.omm_input->device_address() + plan.triangles_offset;
            build_info.triangleArrayStride = sizeof(VkMicromapTriangleEXT);
            build_info.scratchData.deviceAddress =
                    build_data.scratch->device_address() + plan.scratch_offset;

            micromap_assets[plan.entry_idx] = std::move(asset);
        }

        build_data.omm_input->unmap();

        // host writes → micromap build
        VkBufferMemoryBarrier2 barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_MICROMAP_READ_BIT_EXT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT;
        barrier.buffer = build_data.omm_input->handle();
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;
        barriers.push_back(barrier);
    }

    if(all_build_infos.empty()) { return ret; }

    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dep.pBufferMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(params.command_buffer, &dep);

    vkCmdBuildMicromapsEXT(params.command_buffer, static_cast<uint32_t>(all_build_infos.size()),
                           all_build_infos.data());

    // micromap writes → AS build reads
    barriers.clear();
    for(const auto &mesh: params.meshes)
    {
        auto it = context->build_data.find(mesh);
        if(it == context->build_data.end() || !it->second.micromap) { continue; }

        VkBufferMemoryBarrier2 b = {};
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.srcAccessMask = VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT;
        b.srcStageMask = VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT;
        b.dstAccessMask = VK_ACCESS_2_MICROMAP_READ_BIT_EXT;
        b.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        b.buffer = it->second.micromap->handle();
        b.offset = 0;
        b.size = VK_WHOLE_SIZE;
        barriers.push_back(b);
    }
    dep.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
    dep.pBufferMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(params.command_buffer, &dep);

    if(params.query_pool)
    {
        vkCmdWriteTimestamp2(params.command_buffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             params.query_pool.get(), params.query_index_end);
    }
    return ret;
}

}// namespace vierkant
