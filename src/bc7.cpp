#include <cstring>

#include "bc7enc/bc7enc.h"
#include "bc7enc/bc7decomp.h"

//#include <bc7e/bc7e_avx2.h>

#include <vierkant/bc7.hpp>

using duration_t = std::chrono::duration<float>;

namespace vierkant::bc7
{

struct color_quad_u8
{
    uint8_t m_c[4]{};
};

struct init_helper_t
{
    init_helper_t()
    {
        bc7enc_compress_block_init();
//        ispc::bc7e_compress_block_init();
    }
};

inline void get_block(const crocore::Image_<uint8_t>::ConstPtr &img,
                      uint32_t bx, uint32_t by,
                      uint32_t width, uint32_t height,
                      uint32_t elem_size,
                      color_quad_u8 *pPixels)
{
    assert((bx * width + width) <= img->width());
    assert((by * height + height) <= img->height());

    for(uint32_t y = 0; y < height; y++)
    {
        memcpy(pPixels + y * width, img->at(bx * width, by * height + y), width * elem_size);
    }
}

inline uint32_t round4(uint32_t v){ return (v + 3) & ~3; }

bc7::compress_result_t compress(const compress_info_t &compress_info)
{
    static init_helper_t init_helper;

//    assert(std::dynamic_pointer_cast<const crocore::Image_<uint8_t>>(compress_info.image) &&
//           compress_info.image->num_components() == 4);

    auto start_time = std::chrono::steady_clock::now();

    bc7enc_compress_block_params pack_params;
    bc7enc_compress_block_params_init(&pack_params);

//    // faster(wtf!?) alternative bc7e
//    ispc::bc7e_compress_block_params bc7e_params = {};
//    ispc::bc7e_compress_block_params_init_basic(&bc7e_params, true);

    uint32_t width = round4(compress_info.image->width());
    uint32_t height = round4(compress_info.image->height());

    // lowest level will be a 4x4 pixel block
    uint32_t max_levels = static_cast<uint32_t>(
            std::max<int32_t>(0, static_cast<int32_t>(std::log2(std::max(width, height)) - 2)) + 1);
    uint32_t num_levels = compress_info.generate_mipmaps ? max_levels : 1;

    compress_result_t ret = {};
    ret.base_width = width;
    ret.base_height = height;
    ret.levels.resize(num_levels);

    auto source_image = std::dynamic_pointer_cast<const crocore::Image_<uint8_t>>(compress_info.image);
    uint32_t elem_size = source_image->num_components();

    std::vector<std::future<void>> tasks;

    for(auto &blocks : ret.levels)
    {
        source_image = std::dynamic_pointer_cast<const crocore::Image_<uint8_t>>(source_image->resize(width, height));

        uint32_t num_blocks_x = width / 4;
        uint32_t num_blocks_y = height / 4;
        blocks.resize(num_blocks_x * num_blocks_y);

        for(uint32_t by = 0; by < num_blocks_y; by++)
        {
            // function to encode one row of compressed blocks
            auto fn = [by, num_blocks_x, source_image, elem_size, &blocks, &pack_params]
            {
                // scratch-space for block-encoding
                color_quad_u8 pixels[16];

                for(uint32_t bx = 0; bx < num_blocks_x; bx++)
                {
                    get_block(source_image, bx, by, 4, 4, elem_size, pixels);
                    block_t *pBlock = &blocks[bx + by * num_blocks_x];

                    // encode one block
                    bc7enc_compress_block(pBlock, pixels, &pack_params);

                    // ispc::bc7e_compress_blocks(1, pBlock->value, reinterpret_cast<const uint32_t *>(pixels), &bc7e_params);
                }
            };
            if(compress_info.delegate_fn){ tasks.push_back(compress_info.delegate_fn(fn)); }
            else{ fn(); }
        }

        width = std::max<uint32_t>(width / 2, 1);
        height = std::max<uint32_t>(height / 2, 1);

        // round to multiple of 4
        width = round4(width);
        height = round4(height);
    }
    for(const auto &t : tasks){ t.wait(); }

    // timing
    ret.duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

    return ret;
}

}