//
// Created by crocdialer on 12/28/21.
//

#pragma once

#include <chrono>

#include <crocore/Image.hpp>
#include <vierkant/delegate.hpp>

namespace vierkant::bcn
{

enum CompressionMode : uint32_t
{
    BC5 = 0,
    BC7
};

//! 128-bit block encoding 4x4 texels
struct block_t
{
    uint64_t value[2];
};

//! groups encoded blocks by level and base-dimension
struct compress_result_t
{
    CompressionMode mode = BC7;
    uint32_t base_width = 0;
    uint32_t base_height = 0;
    std::vector<std::vector<bcn::block_t>> levels;
    std::chrono::milliseconds duration;
};

//! groups parameters passed to compress() routine
struct compress_info_t
{
    CompressionMode mode = BC7;
    crocore::ImageConstPtr image;
    bool generate_mipmaps = false;
    vierkant::delegate_fn_t delegate_fn;
};

/**
 * @brief   compress an image using a block-compression format.
 *
 * @param   compress_info    a provided bcn::compress_info_t struct
 * @return  a struct grouping mode, encoded blocks and dimensions
 */
compress_result_t compress(const compress_info_t &compress_info);

}// namespace vierkant::bcn
