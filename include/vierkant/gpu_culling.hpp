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

struct gpu_cull_context_t
{
    VkQueue queue = VK_NULL_HANDLE;
    vierkant::CommandPoolPtr command_pool;
    vierkant::CommandBuffer command_buffer;
    vierkant::QueryPoolPtr query_pool;
    uint32_t query_index = 0xFFFFFFFF;

    vierkant::Compute compute;
    glm::uvec3 local_size;
    vierkant::Compute::computable_t computable;
    vierkant::semaphore_submit_info_t semaphore_submit_info = {};

    vierkant::BufferPtr draw_cull_buffer;

    //! draw_cull_result_t buffers
    vierkant::BufferPtr result_buffer;
    vierkant::BufferPtr result_buffer_host;

    uint32_t num_draws = 0;
    vierkant::BufferPtr draws_in;
    vierkant::BufferPtr mesh_draws_in;
    vierkant::BufferPtr mesh_entries_in;

    vierkant::BufferPtr draws_out_main;
    vierkant::BufferPtr draws_out_post;
    vierkant::BufferPtr draws_counts_out_main;
    vierkant::BufferPtr draws_counts_out_post;
};

gpu_cull_context_t create_gpu_cull_context(const vierkant::DevicePtr &device);

struct gpu_cull_params_t
{
    uint32_t occlusion_cull = true;
    uint32_t distance_cull = false;
    uint32_t frustum_cull = true;
    uint32_t lod_enabled = true;
    float lod_base = 15.f;
    float lod_step = 1.5f;
};

struct draw_cull_result_t
{
    uint32_t draw_count = 0;
    uint32_t num_frustum_culled = 0;
    uint32_t num_occlusion_culled = 0;
    uint32_t num_distance_culled = 0;
    uint32_t num_triangles = 0;
};

draw_cull_result_t gpu_cull(vierkant::gpu_cull_context_t &context,
                            vierkant::CameraConstPtr &camera,
                            const vierkant::ImagePtr &depth_pyramid);

}
