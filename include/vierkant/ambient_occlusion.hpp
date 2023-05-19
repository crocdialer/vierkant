//
// Created by crocdialer on 13.04.23.
//

#pragma once

#include <vierkant/Image.hpp>
#include <vierkant/PipelineCache.hpp>
#include <vierkant/descriptor.hpp>

namespace vierkant
{

//! opaque handle owning a ambient_occlusion_context_t
using ambient_occlusion_context_ptr =
        std::unique_ptr<struct ambient_occlusion_context_t, std::function<void(struct ambient_occlusion_context_t *)>>;

struct ambient_occlusion_params_t
{
    vierkant::transform_t camera_transform = {};
    glm::mat4 projection = glm::mat4(1);
    uint32_t num_rays = 0;
    float max_distance = 0.f;
    vierkant::ImagePtr depth_img;
    vierkant::ImagePtr normal_img;
    vierkant::AccelerationStructurePtr top_level;
    VkCommandBuffer commandbuffer;
    std::optional<uint32_t> random_seed;
};

/**
 * @brief   'create_ambient_occlusion_context' will create an ambient_occlusion_context_t
 *          and return an opaque handle to it.
 *
 * @param   device          a provided vierkant::DevicePtr
 * @param   size            provided size of the ambient-occlusion context (and result-image).
 * @param   pipeline_cache  optional vierkant::PipelineCachePtr
 * @return  opaque handle to a ambient_occlusion_context_t.
 */
ambient_occlusion_context_ptr
create_ambient_occlusion_context(const vierkant::DevicePtr &device, const glm::vec2 &size,
                                 const vierkant::PipelineCachePtr &pipeline_cache = nullptr);

/**
 * @brief   'ambient_occlusion' can be used to calculate a fullscreen ambient-occlusion mask.
 *
 * depending on passed parameters one of two implementations will be used:
 * - a pure screenspace approach (SSAO) if no toplevel acceleration-structure is provided
 * - otherwise an approach based on ray-queries will be used (RTAO)
 *
 * @param   context a provided handle to an ambient_occlusion_context.
 * @param   params  a struct grouping parameters.
 * @return  a one channel screenspace-buffer containing an ambient-occlusion mask.
 */
vierkant::ImagePtr ambient_occlusion(const ambient_occlusion_context_ptr &context,
                                     const ambient_occlusion_params_t &params);
}// namespace vierkant
