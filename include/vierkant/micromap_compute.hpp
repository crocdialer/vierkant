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

//! opaque handle owning a micromap_compute_context_t
using micromap_compute_context_handle =
        std::unique_ptr<struct micromap_compute_context_t, std::function<void(struct micromap_compute_context_t *)>>;


struct micromap_compute_params_t
{
    //! a command-buffer to record commands to
    VkCommandBuffer command_buffer;

    //! set of mesh_compute_items
    std::unordered_map<uint64_t, vierkant::MeshConstPtr> mesh_items = {};
};

//! define a typesafe identifier for individual mesh-compute runs
enum class micromap_compute_run_id_t : uint64_t
{
    INVALID = std::numeric_limits<uint64_t>::max()
};

struct micromap_compute_result_t
{
    //! run_id to keep track of results
    micromap_compute_run_id_t run_id = micromap_compute_run_id_t::INVALID;

    // TODO: result ...
};

/**
 * @brief   'create_micromap_compute_context' will create a micromap_compute_context_t and return an opaque handle to it.
 *
 * @param   device          a provided vierkant::DevicePtr
 * @param   pipeline_cache  optional vierkant::PipelineCachePtr
 * @return  opaque handle to a micromap_compute_context_t.
 */
micromap_compute_context_handle
create_micromap_compute_context(const vierkant::DevicePtr &device,
                                const vierkant::PipelineCachePtr &pipeline_cache = nullptr);

/**
 * @brief   'micromap_compute' can be used to create opacity/displacement triangle-micromaps.
 *
 * @param   context a provided handle to a micromap_compute_context.
 * @param   params  a struct grouping parameters.
 * @return  a struct grouping the results of the operation.
 */
micromap_compute_result_t micromap_compute(const micromap_compute_context_handle &context,
                                           const micromap_compute_params_t &params);

}// namespace vierkant
