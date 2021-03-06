//
// Created by crocdialer on 10/28/20.
//

#pragma once

#include "vierkant/GaussianBlur.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Bloom)

class Bloom
{
public:

    struct create_info_t
    {
        //! framebuffer size
        VkExtent3D size = {};

        //! brightness thresh
        glm::vec2 brightness_thresh = glm::vec2(.95f, 1.1f);

        //! blur iterations
        uint32_t num_blur_iterations;

        //! optional pipeline-cache to share shaders and piplines
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
    };

    static BloomUPtr create(const DevicePtr &device, const create_info_t &create_info);

    vierkant::ImagePtr apply(const vierkant::ImagePtr &image, VkQueue queue = VK_NULL_HANDLE);

private:

    Bloom(const DevicePtr &device, const create_info_t &create_info);

    vierkant::Framebuffer m_thresh_framebuffer;

    GaussianBlurPtr m_gaussian_blur;

    vierkant::Renderer::drawable_t m_drawable;

    vierkant::Renderer m_thresh_renderer;
    glm::vec2 m_brightness_thresh;

    VkSpecializationInfo m_specialization_info = {};
    std::array<VkSpecializationMapEntry, 2> m_specialization_entry;
};

}// namespace vierkant

