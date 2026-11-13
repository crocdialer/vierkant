//
// Created by crocdialer on 09/05/26.
//

#pragma once

#include "vierkant/Compute.hpp"
#include "vierkant/Image.hpp"

namespace vierkant
{

class NoiseGenerator
{
public:
    struct create_info_t
    {
        //! output image size, depth > 1 creates a 3D image
        VkExtent3D size = {};

        //! output color-format, needs storage-image support. r: value in [0, 1], gba: optional gradient
        VkFormat color_format = VK_FORMAT_R16_UNORM;

        //! write the gradient of r per unit texture-coordinate to gba. needs a 4-channel color_format
        bool gradient = false;

        //! allocate a mip-chain and regenerate it on each generate. needs linear-blit support for color_format
        bool use_mipmap = false;

        //! optional shared pipeline-cache
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
    };

    ~NoiseGenerator() = default;

    static std::unique_ptr<NoiseGenerator> create(const DevicePtr &device, const create_info_t &create_info);

    /**
     * @brief   generate tiling simplex-noise (psrdnoise) into the output image.
     *
     * @param   commandbuffer   commandbuffer to record into
     * @param   scale           noise-frequency per axis. integer values up to 289 tile seamlessly, others do not.
     *                          z is ignored for 2D images
     * @param   seed            translation of the noise-field
     * @param   alpha           rotation of the gradients in radians, for animation
     * @return  the output image, swizzled to (r, r, r, 1) unless gradients are written
     */
    vierkant::ImagePtr generate(VkCommandBuffer commandbuffer, glm::vec3 scale, glm::vec3 seed, float alpha = 0.f);

private:
    struct params_t
    {
        glm::vec3 scale;
        float alpha;
        glm::vec3 seed;
        uint32_t gradient;
    };

    NoiseGenerator(const DevicePtr &device, const create_info_t &create_info);

    vierkant::DevicePtr m_device;
    vierkant::Compute m_compute;
    vierkant::Compute::computable_t m_computable;
    vierkant::ImagePtr m_image, m_out_image;
    bool m_gradient = false;
};

using NoiseGeneratorPtr = std::shared_ptr<NoiseGenerator>;
using NoiseGeneratorUPtr = std::unique_ptr<NoiseGenerator>;

}// namespace vierkant
