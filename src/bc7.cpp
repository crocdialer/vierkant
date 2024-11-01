#include <cstring>
#include <cmath>
#define RGBCX_IMPLEMENTATION

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "bc7enc/rgbcx.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "bc7enc/bc7decomp.h"
#include "bc7enc/bc7enc.h"

//#include <bc7e/bc7e_avx2.h>

#include <vierkant/bc7.hpp>

using duration_t = std::chrono::duration<float>;

namespace vierkant::bc7
{

struct color_quad_u8
{
    union
    {
        uint8_t m_c[4];
        struct
        {
            uint8_t r, g, b, a;
        };
    };
};

struct init_helper_t
{
    init_helper_t()
    {
        bc7enc_compress_block_init();
        //        ispc::bc7e_compress_block_init();
    }
};

inline void get_block(const crocore::Image_<uint8_t>::ConstPtr &img, uint32_t bx, uint32_t by, bool alpha,
                      color_quad_u8 *pPixels)
{
    constexpr uint32_t width = 4, height = 4;
    assert((bx * width + width) <= img->width());
    assert((by * height + height) <= img->height());

    for(uint32_t y = 0; y < height; ++y)
    {
        //        memcpy(pPixels + y * width, img->at(bx * width, by * height + y), width * sizeof(color_quad_u8));

        for(uint32_t x = 0; x < width; ++x)
        {
            auto in = img->at(bx * width + x, by * height + y);
            color_quad_u8 &out = pPixels[y * width + x];
            out.r = in[0];
            out.g = in[1];
            out.b = in[2];
            out.a = alpha ? in[3] : 255;
        }
    }
}

inline uint32_t round4(uint32_t v) { return (v + 3) & ~3; }

bc7::compress_result_t compress(const compress_info_t &compress_info)
{
    static init_helper_t init_helper;

    assert(std::dynamic_pointer_cast<const crocore::Image_<uint8_t>>(compress_info.image) &&
           compress_info.image->num_components() >= 3);

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
    bool has_alpha = source_image->num_components() == 4;

    std::vector<std::future<void>> tasks;

    for(auto &blocks: ret.levels)
    {
        source_image = std::dynamic_pointer_cast<const crocore::Image_<uint8_t>>(source_image->resize(width, height));

        uint32_t num_blocks_x = width / 4;
        uint32_t num_blocks_y = height / 4;
        blocks.resize(num_blocks_x * num_blocks_y);

        constexpr uint32_t num_rows_per_batch = 4;
        const uint32_t num_batches = (num_blocks_y + num_rows_per_batch - 1) / num_rows_per_batch;

        for(uint32_t i = 0; i < num_batches; i++)
        {
            uint32_t first_row = i * num_rows_per_batch;
            uint32_t end = std::min(num_blocks_y, (i + 1) * num_rows_per_batch);

            // function to encode one row of compressed blocks
            auto fn = [first_row, end, num_blocks_x, source_image, has_alpha, &blocks, &pack_params] {
                // scratch-space for block-encoding
                color_quad_u8 pixels[16];

                for(uint32_t by = first_row; by < end; by++)
                {
                    for(uint32_t bx = 0; bx < num_blocks_x; bx++)
                    {
                        get_block(source_image, bx, by, has_alpha, pixels);
                        block_t *pBlock = &blocks[bx + by * num_blocks_x];

                        // encode one block
                        bc7enc_compress_block(pBlock, pixels, &pack_params);
                    }
                }
            };
            if(compress_info.delegate_fn) { tasks.push_back(compress_info.delegate_fn(fn)); }
            else { fn(); }
        }

        width = std::max<uint32_t>(width / 2, 1);
        height = std::max<uint32_t>(height / 2, 1);

        // round to multiple of 4
        width = round4(width);
        height = round4(height);
    }
    for(const auto &t: tasks) { t.wait(); }

    // timing
    ret.duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

    return ret;
}

}// namespace vierkant::bc7