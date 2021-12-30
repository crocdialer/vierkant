//
// Created by crocdialer on 12/28/21.
//

#pragma once

#include <chrono>

#include <crocore/Image.hpp>
#include <vierkant/delegate.hpp>

namespace vierkant::bc7
{

//! 128-bit block encoding 4x4 texels
struct block_t
{
    uint64_t value[2];
};

//! groups encoded blocks by level and base-dimension
struct compress_result_t
{
    uint32_t base_width = 0;
    uint32_t base_height = 0;
    std::vector<std::vector<bc7::block_t>> levels;
    std::chrono::milliseconds duration;
};

//! groups parameters passed to compress() routine
struct compress_info_t
{
    crocore::ImageConstPtr image;
    bool generate_mipmaps = false;
    vierkant::delegate_fn_t delegate_fn;
};

/**
 * @brief   compress an image using BC7 block-compression.
 *
 * @param   compress_info    a provided bc7::compress_info_t struct
 * @return  a struct grouping encoded blocks and dimension
 */
compress_result_t compress(const compress_info_t &compress_info);

}// namespace vierkant::bc7
