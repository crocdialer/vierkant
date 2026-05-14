//
// Created by crocdialer on 09/05/26.
//

#pragma once

#include "vierkant/Rasterizer.hpp"

namespace vierkant
{

class NoiseGenerator
{
public:
    struct create_info_t
    {
        //! output image size
        VkExtent3D size = {};

        //! output color-format
        VkFormat color_format = VK_FORMAT_R16_UNORM;

        //! optional shared pipeline-cache
        vierkant::PipelineCachePtr pipeline_cache = nullptr;
        vierkant::DescriptorPoolPtr descriptor_pool = nullptr;
    };

    ~NoiseGenerator() = default;

    static std::unique_ptr<NoiseGenerator> create(const DevicePtr &device, const create_info_t &create_info);

    vierkant::ImagePtr generate(VkCommandBuffer commandbuffer, glm::vec2 scale, float seed);

private:
    struct params_t
    {
        glm::vec4 seed;
        glm::vec2 scale;
    };

    NoiseGenerator(const DevicePtr &device, const create_info_t &create_info);

    vierkant::DevicePtr m_device;
    vierkant::Framebuffer m_framebuffer;
    vierkant::ImagePtr m_out_image;
    vierkant::Rasterizer m_renderer;
    vierkant::BufferPtr m_params_buffer;
    vierkant::drawable_t m_drawable;
    VkFormat m_color_format = VK_FORMAT_UNDEFINED;
};

using NoiseGeneratorPtr = std::shared_ptr<NoiseGenerator>;
using NoiseGeneratorUPtr = std::unique_ptr<NoiseGenerator>;

}// namespace vierkant
