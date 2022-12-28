//
// Created by crocdialer on 4/4/21.
//

#pragma once

#include <vierkant/Image.hpp>

namespace vierkant
{

class ImageEffect
{
public:

    ImageEffect() = default;

    virtual ~ImageEffect() = default;

    ImageEffect(const ImageEffect &) = delete;

    ImageEffect &operator=(const ImageEffect &) = delete;

    /**
     * @brief   Apply an image-effect on a provided image, submit to provided queue.
     *
     * @param   image       a provided input-image used as texture-sampler.
     * @param   queue       an optional VkQueue.
     * @param   submit_info an optional VkSubmitInfo struct.
     * @return  a vierkant::ImagePtr containing the result of the operation.
     */
    virtual vierkant::ImagePtr apply(const vierkant::ImagePtr &image,
                                     VkQueue queue,
                                     const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos) = 0;

    /**
     * @brief   Apply an image-effect on a provided image, using a provided command-buffer.
     *
     * @param   image           a provided input-image used as texture-sampler.
     * @param   commandbuffer   a provided command-buffer, currently recording rendering-commands.
     * @return  a vierkant::ImagePtr containing the result of the operation.
     */
    virtual vierkant::ImagePtr apply(const vierkant::ImagePtr &image,
                                     VkCommandBuffer commandbuffer) = 0;
};

}// namespace vierkant
