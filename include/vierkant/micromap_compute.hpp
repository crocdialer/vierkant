//
// Created by crocdialer on 12.02.23.
//

#pragma once

#include <unordered_set>
#include <vierkant/Buffer.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/model/model_loading.hpp>

namespace vierkant
{

using VkMicromapPtr = std::shared_ptr<VkMicromapEXT_T>;

//! opaque handle owning a micromap_compute_context_t
using micromap_compute_context_handle =
        std::unique_ptr<struct micromap_compute_context_t, std::function<void(struct micromap_compute_context_t *)>>;


struct micromap_compute_params_t
{
    VkMicromapTypeEXT micromap_type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT;

    //! a command-buffer to record commands to
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    //! set of meshes to process
    std::unordered_set<vierkant::MeshConstPtr> meshes = {};

    //! CPU OMM cache; entries looked up by {mesh_id, entry_index, material_id}
    const vierkant::model::mesh_omm_cache_t *omm_cache = nullptr;

    vierkant::QueryPoolPtr query_pool = nullptr;
    uint32_t query_index_start = 0, query_index_end = 0;
};

//! define a typesafe identifier for individual mesh-compute runs
enum class micromap_compute_run_id_t : uint64_t
{
    INVALID = std::numeric_limits<uint64_t>::max()
};

struct micromap_asset_t
{
    vierkant::BufferPtr buffer;
    VkMicromapPtr micromap;
    std::vector<VkMicromapUsageEXT> micromap_usages;
    VkDeviceAddress index_buffer_address = 0;
};

struct micromap_compute_result_t
{
    //! run_id to keep track of results
    micromap_compute_run_id_t run_id = micromap_compute_run_id_t::INVALID;

    //! optional micromaps for provided meshes/entries
    std::unordered_map<vierkant::MeshConstPtr, std::vector<std::optional<micromap_asset_t>>> mesh_micromap_assets;
};

/**
 * @brief   'create_micromap_compute_context' will create a micromap_compute_context_t and return an opaque handle to it.
 *
 * @param   device          a provided vierkant::DevicePtr
 * @param   pipeline_cache  ignored, kept for API compatibility
 * @return  opaque handle to a micromap_compute_context_t.
 */
micromap_compute_context_handle
create_micromap_compute_context(const vierkant::DevicePtr &device,
                                const vierkant::PipelineCachePtr &pipeline_cache = nullptr,
                                const VmaPoolPtr &memory_pool = nullptr);

/**
 * @brief   'micromap_compute' uploads CPU-side OMM data from the provided cache and builds VkMicromapEXT objects.
 *
 * @param   context a provided handle to a micromap_compute_context.
 * @param   params  a struct grouping parameters.
 * @return  a struct grouping the results of the operation.
 */
micromap_compute_result_t micromap_compute(const micromap_compute_context_handle &context,
                                           const micromap_compute_params_t &params);

}// namespace vierkant
