//
// Created by crocdialer on 13.11.22.
//

#pragma once

#include <vierkant/math.hpp>
#include <vierkant/Buffer.hpp>
#include <vierkant/Image.hpp>
#include <vierkant/Camera.hpp>
#include "Compute.hpp"

namespace vierkant
{

//! opaque handle owning a gpu_cull_context_t
using gpu_cull_context_ptr = std::unique_ptr<struct gpu_cull_context_t, std::function<void(
        struct gpu_cull_context_t *)>>;

struct gpu_cull_params_t
{
    uint32_t num_draws = 0;

    uint32_t occlusion_cull = true;
    uint32_t distance_cull = false;
    uint32_t frustum_cull = true;
    uint32_t lod_enabled = true;

    //! base screenspace-area for LoD-0
    float lod_base = 0.025f;

    //! step/factor for LoD-selection
    float lod_step = 2.2f;

    //! limit number of LoDs (0: no limit)
    uint32_t max_num_lods = 0;

    VkQueue queue = VK_NULL_HANDLE;
    vierkant::semaphore_submit_info_t semaphore_submit_info = {};

    vierkant::BufferPtr draws_in;
    vierkant::BufferPtr mesh_draws_in;
    vierkant::BufferPtr mesh_entries_in;

    vierkant::BufferPtr draws_out_main;
    vierkant::BufferPtr draws_out_post;
    vierkant::BufferPtr draws_counts_out_main;
    vierkant::BufferPtr draws_counts_out_post;

    vierkant::CameraConstPtr camera;
    vierkant::ImagePtr depth_pyramid;

    vierkant::QueryPoolPtr query_pool;
    uint32_t query_index_start = 0, query_index_end = 0;
};

struct draw_cull_result_t
{
    uint32_t draw_count = 0;
    uint32_t num_frustum_culled = 0;
    uint32_t num_occlusion_culled = 0;
    uint32_t num_distance_culled = 0;
    uint32_t num_triangles = 0;
    uint32_t num_meshlets = 0;
};

struct create_depth_pyramid_params_t
{
    vierkant::ImagePtr depth_map;
    VkQueue queue = VK_NULL_HANDLE;
    vierkant::semaphore_submit_info_t semaphore_submit_info = {};
    vierkant::QueryPoolPtr query_pool;
    uint32_t query_index_start = 0, query_index_end = 0;
};

/**
 * @brief   create_gpu_cull_context is a factory to create an opaque gpu_cull_context_ptr.
 *
 * @param   device          a provided vierkant::Device.
 * @param   pipeline_cache  an optional pipeline_cache.
 * @return  an opaque pointer, owning a gpu_cull_context.
 */
gpu_cull_context_ptr create_gpu_cull_context(const vierkant::DevicePtr &device,
                                             const vierkant::PipelineCachePtr &pipeline_cache = nullptr);

/**
 * @brief   create_depth_pyramid can be used to create a 'hierarchical z-buffer (hzb)' or 'depth-pyramid'.
 *
 * @param   context     a provided vierkant::Device.
 * @param   params      a provided struct with parameters
 * @return  a vierkant::ImagePtr containing the created depth-pyramid
 */
vierkant::ImagePtr create_depth_pyramid(const vierkant::gpu_cull_context_ptr &context,
                                        const create_depth_pyramid_params_t &params);

/**
 * @brief   gpu_cull can be used to cull draw-commands provided in gpu-buffers.
 *
 * @param   context a provided gpu_cull_context_t
 * @param   params  a provided struct with parameters
 * @return  a struct grouping culling-results from previous frame.
 */
draw_cull_result_t gpu_cull(const vierkant::gpu_cull_context_ptr &context, const gpu_cull_params_t &params);

}
