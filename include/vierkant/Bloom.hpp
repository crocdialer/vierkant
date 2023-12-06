//
// Created by crocdialer on 10/28/20.
//

#pragma once

#include "vierkant/GaussianBlur.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Bloom)

class Bloom : public ImageEffect
{
public:

    struct create_info_t
    {
        //! framebuffer size
        VkExtent3D size = {};

        //! hdr-framebuffer format
        VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;

        //! brightness thresh
        glm::vec2 brightness_thresh = glm::vec2(.95f, 1.1f);

        //! blur iterations
        uint32_t num_blur_iterations;

        //! optional pipeline-cache to share shaders and piplines
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
        vierkant::CommandPoolPtr command_pool = nullptr;
    };

    static BloomUPtr create(const DevicePtr &device, const create_info_t &create_info);

    vierkant::ImagePtr apply(const vierkant::ImagePtr &image, VkQueue queue,
                             const std::vector<vierkant::semaphore_submit_info_t> &semaphore_infos) override;

    vierkant::ImagePtr apply(const vierkant::ImagePtr &image,
                             VkCommandBuffer commandbuffer) override;

private:

    Bloom(const DevicePtr &device, const create_info_t &create_info);

    vierkant::DevicePtr m_device;
    vierkant::Framebuffer m_thresh_framebuffer;
    vierkant::CommandPoolPtr m_command_pool;
    vierkant::CommandBuffer m_command_buffer;

    GaussianBlurPtr m_gaussian_blur;

    vierkant::drawable_t m_drawable;

    vierkant::Rasterizer m_thresh_renderer;
    glm::vec2 m_brightness_thresh;
};

}// namespace vierkant

