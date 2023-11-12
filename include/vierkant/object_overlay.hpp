//
// Created by crocdialer on 13.04.23.
//

#pragma once

#include <set>
#include <vierkant/Image.hpp>

namespace vierkant
{

enum class ObjectOverlayMode
{
    BinaryMask,
    Overlay,
    Silhouette
};

//! opaque handle owning an object_overlay_context_t
using object_overlay_context_ptr =
        std::unique_ptr<struct object_overlay_context_t, std::function<void(struct object_overlay_context_t *)>>;

struct object_overlay_params_t
{
    VkCommandBuffer commandbuffer = VK_NULL_HANDLE;

    vierkant::ImagePtr object_id_img;
    std::set<uint32_t> object_ids;

    ObjectOverlayMode mode = ObjectOverlayMode::BinaryMask;
};

/**
 * @brief   'create_object_overlay_context' will create an object_overlay_context_t
 *          and return an opaque handle to it.
 *
 * @param   device          a provided vierkant::DevicePtr
 * @param   size            desired size of the resulting object-overlay image.
 * @return  opaque handle to a object_overlay_context_t.
 */
object_overlay_context_ptr create_object_overlay_context(const vierkant::DevicePtr &device, const glm::vec2 &size);

/**
 * @brief   'object_overlay' can be used to generate a fullscreen object-overlay.
 *
 * @param   context a provided handle to an object_overlay_context.
 * @param   params  a struct grouping parameters.
 * @return  type result-image depends on requested mode (mask, rgb-overlay, rgb-silhouette, ...)
 */
vierkant::ImagePtr object_overlay(const object_overlay_context_ptr &context, const object_overlay_params_t &params);

}// namespace vierkant
