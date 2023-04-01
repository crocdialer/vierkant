//
// Created by crocdialer on 12.02.23.
//

#pragma once

#include <unordered_set>
#include <vierkant/Buffer.hpp>
#include <vierkant/Compute.hpp>
#include <vierkant/Mesh.hpp>

namespace vierkant
{

//! opaque handle owning a mesh_compute_context_t
using mesh_compute_context_ptr =
        std::unique_ptr<struct mesh_compute_context_t, std::function<void(struct mesh_compute_context_t *)>>;


struct mesh_compute_params_t
{
    VkQueue queue = VK_NULL_HANDLE;
    vierkant::semaphore_submit_info_t semaphore_submit_info = {};

    //! set of mesh_compute_items
    std::unordered_map<uint64_t, vierkant::animated_mesh_t> mesh_compute_items = {};

    vierkant::QueryPoolPtr query_pool = nullptr;
    uint32_t query_index_start = 0, query_index_end = 0;
};

//! define a typesafe identifier for individual mesh-compute runs
enum class mesh_compute_run_id_t : uint64_t { INVALID = std::numeric_limits<uint64_t>::max() };

struct mesh_compute_result_t
{
    //! run_id to keep track of results
    mesh_compute_run_id_t run_id = mesh_compute_run_id_t::INVALID;

    //! combined vertex-buffer for all mesh-transformations.
    vierkant::BufferPtr result_buffer = {};

    //! map ids -> offsets into result-buffer
    std::unordered_map<uint64_t, VkDeviceSize> vertex_buffer_offsets = {};
};

/**
 * @brief   'create_mesh_compute_context' will create mesh_compute_context_t and return an opaque handle to it.
 *
 * @param   device          a provided vierkant::DevicePtr
 * @param   result_buffer   optionally provided buffer, used for result vertex-data.
 * @param   pipeline_cache  optional vierkant::PipelineCachePtr
 * @return  opaque handle to a mesh_compute_context_t.
 */
mesh_compute_context_ptr create_mesh_compute_context(const vierkant::DevicePtr &device,
                                                     const vierkant::BufferPtr &result_buffer = nullptr,
                                                     const vierkant::PipelineCachePtr &pipeline_cache = nullptr);

/**
 * @brief   'mesh_compute' can be used to transform mesh-vertices for a list of animated meshes
 *          and provide the result in a combined vertex-buffer with offsets for individual meshes.
 *
 * @param   context a provided handle to a mesh_compute_context.
 * @param   params  a struct grouping parameters.
 * @return  a struct grouping the results of the operation.
 */
mesh_compute_result_t mesh_compute(const mesh_compute_context_ptr &context, const mesh_compute_params_t &params);

}// namespace vierkant
