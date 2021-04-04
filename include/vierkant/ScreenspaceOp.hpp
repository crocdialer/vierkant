//
// Created by crocdialer on 4/4/21.
//

#pragma once

#include <vierkant/Image.hpp>

namespace vierkant
{

class ScreenspaceOp
{
public:

    ScreenspaceOp() = default;

    virtual ~ScreenspaceOp() = default;

    ScreenspaceOp(const ScreenspaceOp &) = delete;

    ScreenspaceOp &operator=(const ScreenspaceOp &) = delete;

    /**
     * @brief   Applies the screenspace-operation using a provided image as input.
     *
     * @param   image       a provided input-image used as texture-sampler.
     * @param   queue       an optional VkQueue.
     * @param   submit_info an optional VkSubmitInof struct.
     * @return  a vierkant::ImagePtr containing the result of the operation.
     */
    virtual vierkant::ImagePtr apply(const vierkant::ImagePtr &image, VkQueue queue, VkSubmitInfo submit_info) = 0;
};

}// namespace vierkant
