
#include <cstring>

#include "bc7enc/bc7enc.h"
#include "bc7enc/bc7decomp.h"

#include <vierkant/bc7.hpp>

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
    init_helper_t(){ bc7enc_compress_block_init(); }
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

bc7::compression_result_t compress(crocore::ImagePtr img)
{
    static init_helper_t init_helper;

    assert(std::dynamic_pointer_cast<crocore::Image_ < uint8_t>>(img) && img->num_components() == 4);

    bc7enc_compress_block_params pack_params;
    bc7enc_compress_block_params_init(&pack_params);

//    constexpr bool perceptual = false;
//    if(!perceptual){ bc7enc_compress_block_params_init_linear_weights(&pack_params); }
    //    pack_params.m_uber_level = std::min(BC7ENC_MAX_UBER_LEVEL, uber_level);

    // consider pow2 upscale vs. crop
    img = img->resize(crocore::next_pow_2(img->width()), crocore::next_pow_2(img->height()));
    auto source_image = std::dynamic_pointer_cast<crocore::Image_ < uint8_t>>(img);
//    source_image.crop((source_image.width() + 3) & ~3, (source_image.height() + 3) & ~3);

    compression_result_t ret = {};
    ret.width = source_image->width();
    ret.height = source_image->height();
    uint32_t num_blocks_x = source_image->width() / 4;
    uint32_t num_blocks_y = source_image->height() / 4;
    ret.blocks.resize(num_blocks_x * num_blocks_y);

    bool has_alpha = false;

    for(uint32_t by = 0; by < num_blocks_y; by++)
    {
        for(uint32_t bx = 0; bx < num_blocks_x; bx++)
        {
            color_quad_u8 pixels[16];

            get_block(source_image, bx, by, 4, 4, pixels);

            if(!has_alpha)
            {
                for(auto &pixel : pixels)
                {
                    if(pixel.m_c[3] < 255)
                    {
                        has_alpha = true;
                        break;
                    }
                }
            }

            block16 *pBlock = &ret.blocks[bx + by * num_blocks_x];

            bc7enc_compress_block(pBlock, pixels, &pack_params);

//            uint32_t mode = ((uint8_t *) pBlock)[0];
//            for (uint32_t m = 0; m <= 7; m++)
//            {
//                if (mode & (1 << m))
//                {
//                    bc7_mode_hist[m]++;
//                }
//            }
        }
    }
    return ret;
}

}