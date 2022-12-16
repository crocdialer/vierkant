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

using gpu_cull_context_ptr = std::unique_ptr<struct gpu_cull_context_t, std::function<void(
        struct gpu_cull_context_t *)>>;

struct gpu_cull_params_t
{
    uint32_t num_draws = 0;

    uint32_t occlusion_cull = true;
    uint32_t distance_cull = false;
    uint32_t frustum_cull = true;
    uint32_t lod_enabled = true;
    float lod_base = 15.f;
    float lod_step = 1.5f;

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
    uint32_t query_index = 0;
};

struct draw_cull_result_t
{
    uint32_t draw_count = 0;
    uint32_t num_frustum_culled = 0;
    uint32_t num_occlusion_culled = 0;
    uint32_t num_distance_culled = 0;
    uint32_t num_triangles = 0;
};

/**
 * @brief   create_gpu_cull_context is a factory to create an opaque gpu_cull_context_ptr.
 *
 * @param   device  a provided vierkant::Device
 * @return  an opaque pointer, owning a gpu_cull_context.
 */
gpu_cull_context_ptr create_gpu_cull_context(const vierkant::DevicePtr &device);

/**
 * @brief   gpu_cull can be used to cull draw-commands provided in gpu-buffers.
 *
 * @param   context a provided gpu_cull_context_t
 * @param   params  a provided gpu_cull_params_t struct
 * @return  a struct grouping culling-results from previous frame.
 */
draw_cull_result_t gpu_cull(const vierkant::gpu_cull_context_ptr &context, const gpu_cull_params_t &params);

}
