//
// Created by crocdialer on 7/2/20.
//

#pragma once

#include "vierkant/Framebuffer.hpp"
#include "vierkant/Rasterizer.hpp"

namespace vierkant
{

struct cube_pipeline_t
{
    vierkant::DevicePtr device;
    vierkant::CommandPoolPtr command_pool;
    vierkant::Framebuffer framebuffer;
    vierkant::Rasterizer renderer;
    vierkant::drawable_t drawable;
    vierkant::ImagePtr color_image;
    vierkant::ImagePtr depth_image;
};

/**
 * @brief   Create assets for a cube-pipeline.
 *
 * @param   device
 * @param   size
 * @param   color_format
 * @param   depth
 * @return  a struct grouping assets for a cube-pipeline.
 */
cube_pipeline_t create_cube_pipeline(const vierkant::DevicePtr &device, uint32_t size, VkFormat color_format,
                                     VkQueue queue, bool depth = false, VkImageUsageFlags usage_flags = 0,
                                     const vierkant::DescriptorPoolPtr &descriptor_pool = nullptr);

/**
 * @brief   Create a procedural cubemap containing a neutral/white lighting-environment
 *
 * @param   device  a provided vierkant::DevicePtr.
 * @param   queue   a provided VkQueue.
 * @param   size    the desired output-size.
 * @param   mipmap  request mipmaps.
 * @param   format  the desired VkFormat
 *
 * @return  a vierkant::ImagePtr holding a cubemap.
 */
vierkant::ImagePtr cubemap_neutral_environment(const vierkant::DevicePtr &device, VkQueue queue, const glm::vec2 &size,
                                               bool mipmap, VkFormat format);

/**
 * @brief   Create a cubemap from an equi-recangular panorama image.
 *
 * @param   panorama_img    the equirectangular panorama.
 * @param   queue   a provided VkQueue.
 * @param   size    the desired output-size.
 * @param   mipmap  request mipmaps.
 * @param   format  the desired VkFormat
 *
 * @return  a vierkant::ImagePtr holding a cubemap.
 */
vierkant::ImagePtr cubemap_from_panorama(const vierkant::DevicePtr &device, const vierkant::ImagePtr &panorama_img,
                                         VkQueue queue, const glm::vec2 &size, bool mipmap, VkFormat format);

/**
 * @brief   Create a diffuse (lambertian brdf) convolution of a provided cubemap.
 *
 * @param   device      a provided vierkant::DevicePtr.
 * @param   cubemap     a provided cubemap.
 * @param   size        the desired output-size.
 * @param   format      the desired VkFormat (i.e. VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_B10G11R11_UFLOAT_PACK32).
 *
 * @return  a cubemap containing a diffuse convolution of the input cubemap.
 */
vierkant::ImagePtr create_convolution_lambert(const vierkant::DevicePtr &device, const vierkant::ImagePtr &cubemap,
                                              uint32_t size, VkFormat format, VkQueue queue);

/**
 * @brief   Create a roughness-cascade of specular (pbr-brdf) convolutions of a provided cubemap.
 *
 * @param   device      a provided vierkant::DevicePtr.
 * @param   cubemap     a provided cubemap.
 * @param   size        the desired output-size.
 * @param   format      the desired VkFormat (i.e. VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_B10G11R11_UFLOAT_PACK32).
 *
 * @return  a cubemap containing in it's mipmap chain a roughness-cascade of specular convolutions of the input cubemap.
 */
vierkant::ImagePtr create_convolution_ggx(const vierkant::DevicePtr &device, const vierkant::ImagePtr &cubemap,
                                          uint32_t size, VkFormat format, VkQueue queue);

/**
 * @brief   create_BRDF_lut can be used to create a texture serving as lookup table for a glossy BRDF.
 *          (NoV, roughness) -> (F, bias)
 *
 * @param   device  a provided vierkant::DevicePtr.
 * @param   queue   a provided VkQueue.
 *
 * @return  a lookup-table containing a mapping of (NoV, roughness) -> (F, bias)
 */
vierkant::ImagePtr create_BRDF_lut(const vierkant::DevicePtr &device, VkQueue queue);

}// namespace vierkant
