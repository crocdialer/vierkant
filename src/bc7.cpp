
#include <cstring>
#include <chrono>

#include "bc7enc/bc7enc.h"
#include "bc7enc/bc7decomp.h"

//#include <bc7e/bc7e.h>

#include <vierkant/bc7.hpp>

using duration_t = std::chrono::duration<float>;

namespace vierkant::bc7
{

struct color_quad_u8
{
    uint8_t m_c[4]{};

    inline color_quad_u8(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        set(r, g, b, a);
    }

    inline explicit color_quad_u8(uint8_t y = 0, uint8_t a = 255)
    {
        set(y, a);
    }

    inline color_quad_u8 &set(uint8_t y, uint8_t a = 255)
    {
        m_c[0] = y;
        m_c[1] = y;
        m_c[2] = y;
        m_c[3] = a;
        return *this;
    }

    inline color_quad_u8 &set(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        m_c[0] = r;
        m_c[1] = g;
        m_c[2] = b;
        m_c[3] = a;
        return *this;
    }

    inline uint8_t &operator[](uint32_t i)
    {
        assert(i < 4);
        return m_c[i];
    }

    inline uint8_t operator[](uint32_t i) const
    {
        assert(i < 4);
        return m_c[i];
    }

    [[nodiscard]] inline int get_luma() const
    {
        return (13938U * m_c[0] + 46869U * m_c[1] + 4729U * m_c[2] + 32768U) >> 16U;
    } // REC709 weightings
};

struct init_helper_t
{
    init_helper_t()
    {
        bc7enc_compress_block_init();
//        ispc::bc7e_compress_block_init();
    }
};

inline void get_block(const crocore::Image_<uint8_t>::Ptr &img,
                      uint32_t bx, uint32_t by,
                      uint32_t width, uint32_t height,
                      color_quad_u8 *pPixels)
{
    assert((bx * width + width) <= img->width());
    assert((by * height + height) <= img->height());

    for(uint32_t y = 0; y < height; y++)
    {
        memcpy(pPixels + y * width, img->at(bx * width, by * height + y), width * sizeof(color_quad_u8));
    }
}

bc7::compression_result_t compress(const crocore::ImagePtr &img, bool generate_mipmaps)
{
    static init_helper_t init_helper;

    assert(std::dynamic_pointer_cast<crocore::Image_<uint8_t>>(img) && img->num_components() == 4);

    bc7enc_compress_block_params pack_params;
    bc7enc_compress_block_params_init(&pack_params);

//    // faster alternative bc7e
//    ispc::bc7e_compress_block_params bc7e_params = {};
//    ispc::bc7e_compress_block_params_init_basic(&bc7e_params, true);

//    crocore::next_pow_2(img->width())
    uint32_t width = (img->width() + 3) & ~3;
    uint32_t height = (img->height() + 3) & ~3;

    // lowest level will be a 4x4 pixel block
    uint32_t max_levels = static_cast<uint32_t>(
            std::max<int32_t>(0, static_cast<int32_t>(std::log2(std::max(width, height)) - 2)) + 1);
    uint32_t num_levels = generate_mipmaps ? max_levels : 1;

    compression_result_t ret = {};
    ret.base_width = width;
    ret.base_height = height;
    ret.levels.resize(num_levels);

    // scratch-space for block-encoding
    color_quad_u8 pixels[16];
    auto source_image = std::dynamic_pointer_cast<crocore::Image_<uint8_t>>(img);

    for(auto &blocks : ret.levels)
    {
        source_image = std::dynamic_pointer_cast<crocore::Image_<uint8_t>>(source_image->resize(width, height));

        uint32_t num_blocks_x = width / 4;
        uint32_t num_blocks_y = height / 4;
        blocks.resize(num_blocks_x * num_blocks_y);

        for(uint32_t by = 0; by < num_blocks_y; by++)
        {
            for(uint32_t bx = 0; bx < num_blocks_x; bx++)
            {
                get_block(source_image, bx, by, 4, 4, pixels);
                block_t *pBlock = &blocks[bx + by * num_blocks_x];

                // encode one block
                bc7enc_compress_block(pBlock, pixels, &pack_params);

//                ispc::bc7e_compress_blocks(1, pBlock->value, reinterpret_cast<const uint32_t *>(pixels), &bc7e_params);
            }
        }

        width = std::max<uint32_t>(width / 2, 1);
        height = std::max<uint32_t>(height / 2, 1);

        // round to multiple of 4
        width = (width + 3) & ~3;
        height = (height + 3) & ~3;
    }
    return ret;
}

}