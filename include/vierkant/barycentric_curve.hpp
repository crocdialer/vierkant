/*
Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "math.hpp"

namespace vierkant
{

//! retrieve the number of micro-triangles for a given triangle subdivision-level
static constexpr inline uint32_t num_micro_triangles(uint32_t num_levels) { return 1 << (num_levels << 1u); }

//	static constexpr inline uint32_t GetBitCount(ommFormat vmFormat) {
//		OMM_ASSERT(vmFormat != ommFormat_INVALID);
//		static_assert((uint32_t)ommFormat_OC1_2_State == 1);
//		static_assert((uint32_t)ommFormat_OC1_4_State == 2);
//		static_assert((uint32_t)ommFormat_MAX_NUM == 3);
//		return (uint32_t)vmFormat;
//	}

// BEGIN - From Optix SDK 7.6.0
// Extract even bits
static uint32_t extract_even_bits(uint32_t x)
{
    x &= 0x55555555;
    x = (x | (x >> 1)) & 0x33333333;
    x = (x | (x >> 2)) & 0x0f0f0f0f;
    x = (x | (x >> 4)) & 0x00ff00ff;
    x = (x | (x >> 8)) & 0x0000ffff;
    return x;
}

// Calculate exclusive prefix or (log(n) XOR's and SHF's)
static uint32_t prefixEor(uint32_t x)
{
    x ^= x >> 1;
    x ^= x >> 2;
    x ^= x >> 4;
    x ^= x >> 8;
    return x;
}

// Convert distance along the curve to discrete barycentrics
static void index2dbary(uint32_t index, uint32_t &u, uint32_t &v, uint32_t &w)
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

// Compute barycentrics for micro triangle
static void index2bary(uint32_t index, uint32_t subdivision_level, glm::vec2 &uv0, glm::vec2 &uv1, glm::vec2 &uv2)
{
    if(subdivision_level == 0)
    {
        uv0 = {0, 0};
        uv1 = {1, 0};
        uv2 = {0, 1};
        return;
    }

    uint32_t iu, iv, iw;
    index2dbary(index, iu, iv, iw);

    // we need to only look at "level" bits
    iu = iu & ((1 << subdivision_level) - 1);
    iv = iv & ((1 << subdivision_level) - 1);
    iw = iw & ((1 << subdivision_level) - 1);

    bool upright = (iu & 1) ^ (iv & 1) ^ (iw & 1);
    if(!upright)
    {
        iu = iu + 1;
        iv = iv + 1;
    }

    const uint32_t levelScalei = ((127u - subdivision_level) << 23);
    const float levelScale = reinterpret_cast<const float &>(levelScalei);

    // scale the barycentic coordinate to the global space/scale
    float du = 1.f * levelScale;
    float dv = 1.f * levelScale;

    // scale the barycentic coordinate to the global space/scale
    float u = (float) iu * levelScale;
    float v = (float) iv * levelScale;

    if(!upright)
    {
        du = -du;
        dv = -dv;
    }

    uv0 = {u, v};
    uv1 = {u + du, v};
    uv2 = {u, v + dv};
}
// END - From Optix SDK 7.6.0

//! reference code from vulkan-specification
static uint32_t bary2index(float u, float v, uint32_t level)
{
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    uint32_t iu, iv, iw;

    // Quantize barycentric coordinates
    float fu = u * (1u << level);
    float fv = v * (1u << level);

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

//// BEGIN - From DMM SDK
//// Compute 2 16-bit prefix XORs in a 32-bit register
//static inline uint32_t prefixEor2(uint32_t x)
//{
//    x ^= (x >> 1) & 0x7fff7fff;
//    x ^= (x >> 2) & 0x3fff3fff;
//    x ^= (x >> 4) & 0x0fff0fff;
//    x ^= (x >> 8) & 0x00ff00ff;
//    return x;
//}
//
//// Interleave 16 even bits from x with 16 odd bits from y
//static inline uint32_t interleaveBits2(uint32_t x, uint32_t y)
//{
//    x = (x & 0xffff) | (y << 16);
//    x = ((x >> 8) & 0x0000ff00) | ((x << 8) & 0x00ff0000) | (x & 0xff0000ff);
//    x = ((x >> 4) & 0x00f000f0) | ((x << 4) & 0x0f000f00) | (x & 0xf00ff00f);
//    x = ((x >> 2) & 0x0c0c0c0c) | ((x << 2) & 0x30303030) | (x & 0xc3c3c3c3);
//    x = ((x >> 1) & 0x22222222) | ((x << 1) & 0x44444444) | (x & 0x99999999);
//
//    return x;
//}
//
//// Convert distance along the curve to discrete barycentrics
//static uint32_t dbary2index(uint32_t u, uint32_t v, uint32_t w, uint32_t level)
//{
//    const uint32_t coordMask = ((1U << level) - 1);
//
//    uint32_t b0 = ~(u ^ w) & coordMask;
//    uint32_t t = (u ^ v) & b0;//  (equiv: (~u & v & ~w) | (u & ~v & w))
//    uint32_t c = (((u & v & w) | (~u & ~v & ~w)) & coordMask) << 16;
//    uint32_t f = prefixEor2(t | c) ^ u;
//    uint32_t b1 = (f & ~b0) | t;// equiv: (~u & v & ~w) | (u & ~v & w) | (f0 & u & ~w) | (f0 & ~u & w))
//
//    return interleaveBits2(b0, b1);// 13 instructions
//}
//// END - From DMM SDK
//
//static uint32_t bary2index(glm::vec2 bc, uint32_t level, bool &isUpright)
//{
//    float numSteps = float(1u << level);
//    uint32_t iu = uint32_t(numSteps * bc.x);
//    uint32_t iv = uint32_t(numSteps * bc.y);
//    uint32_t iw = uint32_t(numSteps * (1.f - bc.x - bc.y));
//    isUpright = (iu & 1) ^ (iv & 1) ^ (iw & 1);
//    return dbary2index(iu, iv, iw, level);
//}

//// Returns a micro-triangle for the index on the curve.
//static inline Triangle GetMicroTriangle(const Triangle &t, uint32_t index, uint32_t subdivisionLevel)
//{
//    glm::vec2 uv0;
//    glm::vec2 uv1;
//    glm::vec2 uv2;
//    bird::index2bary(index, subdivisionLevel, uv0, uv1, uv2);
//
//    const glm::vec2 uP0 = omm::InterpolateTriangleUV(InitBarycentrics(uv0), t);
//    const glm::vec2 uP1 = omm::InterpolateTriangleUV(InitBarycentrics(uv1), t);
//    const glm::vec2 uP2 = omm::InterpolateTriangleUV(InitBarycentrics(uv2), t);
//
//    return Triangle(uP0, uP1, uP2);
//}

}// namespace vierkant
