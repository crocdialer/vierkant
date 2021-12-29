//
// Created by crocdialer on 12/28/21.
//

#pragma once

#include <crocore/Image.hpp>

namespace vierkant::bc7
{

//! 128-bit block encoding 4x4 texels
struct block16
{
    uint64_t m_vals[2];
};

//! groups encoded blocks and dimension
struct compression_result_t
{
    uint32_t width;
    uint32_t height;
    std::vector<bc7::block16> blocks;
};

/**
 * @brief   compress an image using BC7 block-compression.
 *
 * @param   img a provided crocore::ImagePtr
 * @return  a struct grouping encoded blocks and dimension
 */
compression_result_t compress(crocore::ImagePtr img);

}// namespace vierkant::bc7
