#define BOOST_TEST_MAIN

#include "test_context.hpp"

#include <vierkant/bc7.hpp>

// 4x4 black/white checkerboard RGBA
uint32_t checker_board_4x4[] = {0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0xFF000000,
                                0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF,
                                0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0xFF000000,
                                0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF};

inline uint32_t round4(uint32_t val){ return (val + 3) & ~3; }

//! helper to derive number of mip-levels (all levels need to be devisible by 4 and at least 4x4 texels)
inline uint32_t num_levels(uint32_t width, uint32_t height)
{
    width = round4(width);
    height = round4(height);
    return static_cast<uint32_t>(
            std::max<int32_t>(0, static_cast<int32_t>(std::log2(std::max(width, height)) - 2)) + 1);
}

inline uint32_t num_blocks(uint32_t base_width, uint32_t base_height, uint32_t level)
{
    base_width = round4(base_width);
    base_height = round4(base_height);

    for(uint32_t l = 0; l < level; ++l)
    {
        base_width = std::max<uint32_t>(base_width / 2, 1);
        base_height = std::max<uint32_t>(base_height / 2, 1);

        // round to multiple of 4
        base_width = round4(base_width);
        base_height = round4(base_height);
    }
    base_width /= 4;
    base_height /= 4;
    return base_width * base_height;
}

void check(const vierkant::bc7::compress_info_t &compress_info, const vierkant::bc7::compress_result_t &compress_result)
{
    BOOST_CHECK(compress_info.image);

    uint32_t width = compress_info.image->width();
    uint32_t height = compress_info.image->height();

    BOOST_CHECK(compress_result.duration > std::chrono::milliseconds(0));
    BOOST_CHECK_EQUAL(compress_result.base_width, round4(width));
    BOOST_CHECK_EQUAL(compress_result.base_height, round4(height));
    BOOST_CHECK_EQUAL(compress_result.levels.size(), compress_info.generate_mipmaps ? num_levels(width, height) : 1);

    for(uint32_t l = 0; l < compress_result.levels.size(); ++l)
    {
        BOOST_CHECK_EQUAL(compress_result.levels[l].size(), num_blocks(width, height, l));
    }
}

BOOST_AUTO_TEST_CASE(basic)
{
    auto img = crocore::Image_<uint8_t>::create(reinterpret_cast<uint8_t *>(checker_board_4x4), 4, 4, 4);
    BOOST_CHECK(img);

    uint32_t width = 512, height = 256;
    auto img8u = img->resize(width, height);

    vierkant::bc7::compress_info_t compress_info = {};
    compress_info.image = img8u;
    auto compress_result = vierkant::bc7::compress(compress_info);
    check(compress_info, compress_result);
}

BOOST_AUTO_TEST_CASE(mips)
{
    auto img = crocore::Image_<uint8_t>::create(reinterpret_cast<uint8_t *>(checker_board_4x4), 4, 4, 4);
    BOOST_CHECK(img);

    uint32_t width = 512, height = 256;
    auto img8u = img->resize(width, height);

    vierkant::bc7::compress_info_t compress_info = {};
    compress_info.image = img8u;
    compress_info.generate_mipmaps = true;
    auto compress_result = vierkant::bc7::compress(compress_info);
    check(compress_info, compress_result);
}

BOOST_AUTO_TEST_CASE(odd_size)
{
    auto img = crocore::Image_<uint8_t>::create(reinterpret_cast<uint8_t *>(checker_board_4x4), 4, 4, 4);
    BOOST_CHECK(img);

    uint32_t width = 123, height = 81;
    auto img8u = img->resize(width, height);

    vierkant::bc7::compress_info_t compress_info = {};
    compress_info.image = img8u;
    compress_info.generate_mipmaps = true;
    auto compress_result = vierkant::bc7::compress(compress_info);
    check(compress_info, compress_result);
}