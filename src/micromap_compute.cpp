//
// Created by crocdialer on 27.11.23.
//

#define VK_NO_PROTOTYPES
#include <volk.h>

#include <vierkant/barycentric_indexing.hpp>
#include <vierkant/micromap_compute.hpp>
#include <vierkant/shaders.hpp>

namespace vierkant
{

struct mesh_build_data_t
{
    vierkant::BufferPtr data;
    vierkant::BufferPtr triangles;
    vierkant::BufferPtr indices;
    vierkant::BufferPtr scratch;
};

struct micromap_compute_context_t
{
    vierkant::DevicePtr device;
    vierkant::Compute compute;
    glm::uvec3 micromap_compute_local_size{};
    vierkant::Compute::computable_t micromap_computable;

    vierkant::BufferPtr staging_buffer;
    vierkant::BufferPtr params_ubo_buffer;

    // tmp during build
    std::unordered_map<vierkant::MeshConstPtr, mesh_build_data_t> build_data;

    uint64_t run_id = 0;
};

struct alignas(16) micromap_params_t
{
    uint32_t num_triangles;
    uint32_t num_subdivisions;
    uint32_t format;//VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT
    float alpha_cutoff;

    VkDeviceAddress vertex_in;
    VkDeviceAddress index_in;
    VkDeviceAddress micromap_data_out;
    VkDeviceAddress micromap_triangle_out;
    VkDeviceAddress micromap_indices_out;
};

micromap_compute_context_handle create_micromap_compute_context(const DevicePtr &device,
                                                                const PipelineCachePtr &pipeline_cache)
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
    return ret;
}

micromap_compute_result_t micromap_compute(const micromap_compute_context_handle &context,
                                           const micromap_compute_params_t &params)
{
    const auto num_micro_triangle_bits = static_cast<uint32_t>(params.micromap_format);
    const uint32_t num_micro_triangles = vierkant::num_micro_triangles(params.num_subdivisions);
    // TODO: timestamp start

    micromap_compute_result_t ret;

    for(const auto &mesh: params.meshes)
    {
        size_t num_entries = mesh ? mesh->entries.size() : 0;

        auto &micromap_assets = ret.mesh_micromap_assets[mesh];
        micromap_assets.resize(num_entries);

        // per entry
        std::vector<VkMicromapUsageEXT> micromap_usages(num_entries);
        std::vector<VkMicromapBuildInfoEXT> micromap_build_infos(num_entries);

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

        for(uint32_t i = 0; i < num_entries; ++i)
        {
            buffer_offsets[i] = buffer_size_sum;

            const auto &entry = mesh->entries[i];
            const auto &lod_0 = entry.lods.front();
            //            const auto &material = mesh->materials[entry.material_index];

            const size_t num_triangles = lod_0.num_indices / 3;

            {
                size_t num_data_bytes = (num_triangles * num_micro_triangles * num_micro_triangle_bits) / 8;
                size_t num_triangle_data_bytes = num_triangles * sizeof(VkMicromapTriangleEXT);
                size_t num_index_data_bytes = num_triangles * sizeof(uint32_t);

                buffer_sizes[i].data = num_data_bytes;
                buffer_size_sum.data += num_data_bytes;
                buffer_sizes[i].index_data = num_index_data_bytes;
                buffer_size_sum.index_data += num_index_data_bytes;
                buffer_sizes[i].triangle_data = num_triangle_data_bytes;
                buffer_size_sum.triangle_data += num_triangle_data_bytes;
            }

            // per entry
            auto &micromap_usage = micromap_usages[i];
            micromap_usage.count = num_triangles;
            micromap_usage.subdivisionLevel = params.num_subdivisions;
            micromap_usage.format = params.micromap_format;

            // per entry
            auto &micromap_build_info = micromap_build_infos[i];
            micromap_build_info.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT;
            micromap_build_info.flags =
                    VK_BUILD_MICROMAP_PREFER_FAST_TRACE_BIT_EXT | VK_BUILD_MICROMAP_ALLOW_COMPACTION_BIT_EXT;
            micromap_build_info.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;
            micromap_build_info.usageCountsCount = 1;
            micromap_build_info.pUsageCounts = &micromap_usage;

            // query memory requirements
            VkMicromapBuildSizesInfoEXT micromap_size_info = {};
            micromap_size_info.sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT;
            vkGetMicromapBuildSizesEXT(context->device->handle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                       &micromap_build_info, &micromap_size_info);

            // store micromap/scratch-sizes
            buffer_sizes[i].micromap = micromap_size_info.micromapSize;
            buffer_size_sum.micromap += micromap_size_info.micromapSize;
            buffer_sizes[i].scratch = micromap_size_info.buildScratchSize;
            buffer_size_sum.scratch += micromap_size_info.buildScratchSize;
        }// entries

        auto &build_data = context->build_data[mesh];
        micromap_asset_t micromap_asset = {};

        // resize buffers
        {
            vierkant::Buffer::create_info_t scratch_buffer_info = {};
            scratch_buffer_info.device = context->device;
            scratch_buffer_info.num_bytes = buffer_size_sum.scratch;
            scratch_buffer_info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            scratch_buffer_info.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            build_data.scratch = vierkant::Buffer::create(scratch_buffer_info);

            // stores input-opacity data
            vierkant::Buffer::create_info_t micromap_input_buffer_format = {};
            micromap_input_buffer_format.device = context->device;
            micromap_input_buffer_format.num_bytes = buffer_size_sum.data;
            micromap_input_buffer_format.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT;
            micromap_input_buffer_format.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            micromap_input_buffer_format.pool = nullptr;
            build_data.data = vierkant::Buffer::create(micromap_input_buffer_format);

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
            micromap_asset.buffer = vierkant::Buffer::create(micromap_buffer_info);
        }

        for(uint32_t i = 0; i < num_entries; ++i)
        {
            const auto &buffer_size = buffer_sizes[i];
            const auto &buffer_offset = buffer_offsets[i];

            // create blank micromap
            VkMicromapCreateInfoEXT micromap_create_info = {};
            micromap_create_info.sType = VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT;
            micromap_create_info.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;
            micromap_create_info.size = buffer_size.micromap;
            micromap_create_info.buffer = micromap_asset.buffer->handle();
            micromap_create_info.offset = buffer_offset.micromap;

            VkMicromapEXT handle;
            vkCheck(vkCreateMicromapEXT(context->device->handle(), &micromap_create_info, nullptr, &handle),
                    "could not create micromap");
            micromap_asset.micromap = {handle, [device = context->device](VkMicromapEXT p) {
                                           vkDestroyMicromapEXT(device->handle(), p, nullptr);
                                       }};

            // TODO: run compute-pipelines to generate opacity-input data

            // finalize build-information
            auto &micromap_build_info = micromap_build_infos[i];
            micromap_build_info.dstMicromap = micromap_asset.micromap.get();
            micromap_build_info.scratchData.deviceAddress =
                    build_data.scratch->device_address() + buffer_offset.scratch;
            micromap_build_info.data.deviceAddress = build_data.data->device_address() + buffer_offset.data;
            micromap_build_info.triangleArray.deviceAddress =
                    build_data.triangles->device_address() + buffer_offset.triangle_data;
            micromap_build_info.triangleArrayStride = sizeof(VkMicromapTriangleEXT);
        }

        // build micromaps for all entries
        vkCmdBuildMicromapsEXT(params.command_buffer, micromap_build_infos.size(), micromap_build_infos.data());

        // TODO: that will be needed later, during acceleration-structure builds
        //
        //        VkAccelerationStructureTrianglesOpacityMicromapEXT triangles_micromap = {};
        //        triangles_micromap.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT;
        //        triangles_micromap.micromap = VK_NULL_HANDLE;
        //        triangles_micromap.indexBuffer.deviceAddress = 0;
        //        triangles_micromap.indexStride = vierkant::num_bytes(VK_INDEX_TYPE_UINT32);
        //        triangles_micromap.indexType = VK_INDEX_TYPE_UINT32;
    }// meshes

    // TODO: timestamp end
    return ret;
}

}// namespace vierkant