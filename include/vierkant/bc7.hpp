//
// Created by crocdialer on 12/28/21.
//

#pragma once

#include <crocore/Image.hpp>

namespace vierkant::bc7
{

//! 128-bit block encoding 4x4 texels
struct block_t
{
    uint64_t value[2];
};

//! groups encoded blocks by level and base-dimension
struct compression_result_t
{
    uint32_t base_width;
    uint32_t base_height;
    std::vector<std::vector<bc7::block_t>> levels;
};

/**
 * @brief   compress an image using BC7 block-compression.
 *
 * @param   img a provided crocore::ImagePtr
 * @return  a struct grouping encoded blocks and dimension
 */
compression_result_t compress(const crocore::ImagePtr& img, bool generate_mipmaps);

}// namespace vierkant::bc7
