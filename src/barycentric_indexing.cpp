//
// Created by crocdialer on 20.11.23.
//

#include <algorithm>
#include <vierkant/barycentric_indexing.hpp>

namespace vierkant
{

//! even bits
static inline uint32_t extract_even_bits(uint32_t x)
{
    x &= 0x55555555;
    x = (x | (x >> 1)) & 0x33333333;
    x = (x | (x >> 2)) & 0x0f0f0f0f;
    x = (x | (x >> 4)) & 0x00ff00ff;
    x = (x | (x >> 8)) & 0x0000ffff;
    return x;
}

//! exclusive prefix or (log(n) XOR's and SHF's)
static inline uint32_t prefixEor(uint32_t x)
{
    x ^= x >> 1;
    x ^= x >> 2;
    x ^= x >> 4;
    x ^= x >> 8;
    return x;
}

//! convert distance along the curve to discrete barycentrics
static inline void index2dbary(uint32_t index, uint32_t &u, uint32_t &v, uint32_t &w)
{
    uint32_t b0 = extract_even_bits(index);
    uint32_t b1 = extract_even_bits(index >> 1);

    uint32_t fx = prefixEor(b0);
    uint32_t fy = prefixEor(b0 & ~b1);

    uint32_t t = fy ^ b1;

    u = (fx & ~t) | (b0 & ~t) | (~b0 & ~fx & t);
    v = fy ^ b0;
    w = (~fx & ~t) | (b0 & ~t) | (~b0 & fx & t);
}

void index2bary(uint32_t index, uint32_t level, glm::vec2 &uv0, glm::vec2 &uv1, glm::vec2 &uv2)
{
    if(level == 0)
    {
        uv0 = {0, 0};
        uv1 = {1, 0};
        uv2 = {0, 1};
        return;
    }
    uint32_t iu, iv, iw;
    index2dbary(index, iu, iv, iw);

    // consider only "level" bits
    iu = iu & ((1 << level) - 1);
    iv = iv & ((1 << level) - 1);
    iw = iw & ((1 << level) - 1);

    bool upright = (iu & 1) ^ (iv & 1) ^ (iw & 1);

    if(!upright)
    {
        iu = iu + 1;
        iv = iv + 1;
    }

    // matches a reinterpret-cast uint32_t -> float, but avoids 'dereferencing type-punned pointer'
    union
    {
        uint32_t ui;
        float f;
    } level_scale{(127u - level) << 23};

    // scale the barycentic coordinate to the global space/scale
    float du = 1.f * level_scale.f;
    float dv = 1.f * level_scale.f;

    // scale the barycentic coordinate to the global space/scale
    float u = (float) iu * level_scale.f;
    float v = (float) iv * level_scale.f;

    if(!upright)
    {
        du = -du;
        dv = -dv;
    }
    uv0 = {u, v};
    uv1 = {u + du, v};
    uv2 = {u, v + dv};
}

uint32_t bary2index(const glm::vec2 &uv, uint32_t level)
{
    float u = std::clamp(uv.x, 0.0f, 1.0f);
    float v = std::clamp(uv.y, 0.0f, 1.0f);

    uint32_t iu, iv, iw;

    // Quantize barycentric coordinates
    float fu = u * static_cast<float>(1u << level);
    float fv = v * static_cast<float>(1u << level);

    iu = (uint32_t) fu;
    iv = (uint32_t) fv;

    float uf = fu - float(iu);
    float vf = fv - float(iv);

    if(iu >= (1u << level)) iu = (1u << level) - 1u;
    if(iv >= (1u << level)) iv = (1u << level) - 1u;

    uint32_t iuv = iu + iv;

    if(iuv >= (1u << level)) iu -= iuv - (1u << level) + 1u;

    iw = ~(iu + iv);

    if(uf + vf >= 1.0f && iuv < (1u << level) - 1u) --iw;

    uint32_t b0 = ~(iu ^ iw);
    b0 &= ((1u << level) - 1u);
    uint32_t t = (iu ^ iv) & b0;

    uint32_t f = t;
    f ^= f >> 1u;
    f ^= f >> 2u;
    f ^= f >> 4u;
    f ^= f >> 8u;
    uint32_t b1 = ((f ^ iu) & ~b0) | t;

    // Interleave bits
    b0 = (b0 | (b0 << 8u)) & 0x00ff00ffu;
    b0 = (b0 | (b0 << 4u)) & 0x0f0f0f0fu;
    b0 = (b0 | (b0 << 2u)) & 0x33333333u;
    b0 = (b0 | (b0 << 1u)) & 0x55555555u;
    b1 = (b1 | (b1 << 8u)) & 0x00ff00ffu;
    b1 = (b1 | (b1 << 4u)) & 0x0f0f0f0fu;
    b1 = (b1 | (b1 << 2u)) & 0x33333333u;
    b1 = (b1 | (b1 << 1u)) & 0x55555555u;

    return b0 | (b1 << 1u);
}
}// namespace vierkant