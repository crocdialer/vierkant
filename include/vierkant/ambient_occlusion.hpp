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
    float near = 0.f, far = 0.f;
    uint32_t num_rays = 0;
    float max_distance = 0.f;
    vierkant::ImagePtr depth_img;
    vierkant::ImagePtr normal_img;
    vierkant::AccelerationStructurePtr top_level;
    VkCommandBuffer commandbuffer;
    std::optional<uint32_t> random_seed;
};

ambient_occlusion_context_ptr
create_ambient_occlusion_context(const vierkant::DevicePtr &device, const glm::vec2 &size,
                                 const vierkant::PipelineCachePtr &pipeline_cache = nullptr);

vierkant::ImagePtr ambient_occlusion(const ambient_occlusion_context_ptr &context,
                                     const ambient_occlusion_params_t &params);
}// namespace vierkant
