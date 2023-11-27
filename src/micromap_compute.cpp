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

struct micromap_compute_context_t
{
    vierkant::DevicePtr device;
    vierkant::Compute compute;
    glm::uvec3 micromap_compute_local_size{};
    vierkant::Compute::computable_t micromap_computable;
    vierkant::BufferPtr staging_buffer;
    vierkant::BufferPtr result_buffer;
    uint64_t run_id = 0;
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
    micromap_compute_result_t ret;

    for(const auto &mesh: params.meshes)
    {
        size_t num_entries = mesh ? mesh->entries.size() : 0;

        for(uint32_t i = 0; i < num_entries; ++i)
        {
            const auto &entry = mesh->entries[i];
            const auto &lod_0 = entry.lods.front();
            //            const auto &material = mesh->materials[entry.material_index];

            auto num_micro_triangle_bits = static_cast<uint32_t>(params.micromap_format);
            micromap_asset_t micromap_asset = {};

            const size_t num_triangles = lod_0.num_indices / 3;
            size_t num_data_bytes = num_triangles * vierkant::num_micro_triangles(params.num_subdivisions) *
                                    num_micro_triangle_bits / 8;

            // TODO: run compute-shader to generate micromap-data (sample opacity for all micro-triangles)

            // stores input-opacity data
            vierkant::Buffer::create_info_t micromap_input_buffer_format = {};
            micromap_input_buffer_format.device = context->device;
            micromap_input_buffer_format.num_bytes = num_data_bytes;
            micromap_input_buffer_format.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                 VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT;
            micromap_input_buffer_format.mem_usage = VMA_MEMORY_USAGE_GPU_ONLY;
            micromap_input_buffer_format.pool = nullptr;
            micromap_asset.data = vierkant::Buffer::create(micromap_input_buffer_format);

            // populate buffer with array of VkMicromapTriangleEXT
            micromap_input_buffer_format.num_bytes = num_triangles * sizeof(VkMicromapTriangleEXT);
            micromap_asset.triangles = vierkant::Buffer::create(micromap_input_buffer_format);

            VkMicromapUsageEXT micromap_usage = {};
            micromap_usage.count = num_triangles;
            micromap_usage.subdivisionLevel = params.num_subdivisions;
            micromap_usage.format = params.micromap_format;

            VkMicromapBuildInfoEXT micromap_build_info = {};
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

            // stores result-micromap
            micromap_asset.buffer = vierkant::Buffer::create(
                    context->device, nullptr, std::max<uint64_t>(micromap_size_info.micromapSize, 1 << 12U),
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT,
                    VMA_MEMORY_USAGE_GPU_ONLY, nullptr);

            // required scratch-buffer for micromap-building
            micromap_asset.scratch = vierkant::Buffer::create(
                    context->device, nullptr, std::max<uint64_t>(micromap_size_info.buildScratchSize, 1 << 12U),
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY, nullptr);

            // create blank micromap
            VkMicromapCreateInfoEXT micromap_create_info = {};
            micromap_create_info.sType = VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT;
            micromap_create_info.type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;
            micromap_create_info.size = micromap_size_info.micromapSize;
            micromap_create_info.buffer = micromap_asset.buffer->handle();
            micromap_create_info.offset = 0;

            VkMicromapEXT handle;
            vkCheck(vkCreateMicromapEXT(context->device->handle(), &micromap_create_info, nullptr, &handle),
                    "could not create micromap");
            micromap_asset.micromap = {handle, [device = context->device](VkMicromapEXT p) {
                                           vkDestroyMicromapEXT(device->handle(), p, nullptr);
                                       }};

            // build micromap
            micromap_build_info.dstMicromap = micromap_asset.micromap.get();
            micromap_build_info.scratchData.deviceAddress = micromap_asset.scratch->device_address();
            micromap_build_info.data.deviceAddress = micromap_asset.data->device_address();
            micromap_build_info.triangleArray.deviceAddress = micromap_asset.triangles->device_address();
            micromap_build_info.triangleArrayStride = sizeof(VkMicromapTriangleEXT);
            vkCmdBuildMicromapsEXT(params.command_buffer, 1, &micromap_build_info);

            //            optional_micromap = std::move(micromap_asset);
            //            VkAccelerationStructureTrianglesOpacityMicromapEXT triangles_micromap = {};
            //            triangles_micromap.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT;
            //            triangles_micromap.indexBuffer
            //            triangles_micromap.indexStride
            //            triangles_micromap.indexType
        }
    }

    return ret;
}

}// namespace vierkant