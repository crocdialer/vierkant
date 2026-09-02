#pragma once

#include <vierkant/Image.hpp>

namespace vierkant
{

//! host-side contents of a cubemap. one blob per mip-level, holding the 6 faces back-to-back.
struct cubemap_data_t
{
    //! texel-format of the stored blobs
    VkFormat format = VK_FORMAT_UNDEFINED;

    //! edge-length of the base-level
    uint32_t size = 0;

    //! per mip-level: 6 faces of raw texels
    std::vector<std::vector<uint8_t>> levels;
};

/**
 * @brief   read a cubemap back into host-memory.
 *
 * @param   cubemap a 6-layer image, created with VK_IMAGE_USAGE_TRANSFER_SRC_BIT.
 * @param   queue   a VkQueue used for the transfer.
 * @return  the host-side contents, empty if 'cubemap' is not a readable cubemap.
 */
cubemap_data_t download_cubemap(const vierkant::ImagePtr &cubemap, VkQueue queue);

/**
 * @brief   create a sampled cubemap from host-side contents.
 *
 * @param   device  a provided vierkant::DevicePtr.
 * @param   data    host-side contents, as returned by download_cubemap.
 * @param   queue   a VkQueue used for the transfer.
 * @return  a cubemap in VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, or nullptr if 'data' is empty.
 */
vierkant::ImagePtr upload_cubemap(const vierkant::DevicePtr &device, const cubemap_data_t &data, VkQueue queue);

}// namespace vierkant
