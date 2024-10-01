//
// Created by crocdialer on 05.02.23.
//

#pragma once

#include <functional>
#include <string_view>

namespace vierkant
{

inline uint32_t murmur3_fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline uint64_t murmur3_fmix64(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdLLU;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53LLU;
    k ^= k >> 33;
    return k;
}

// Generate a random uint32_t from two uint32_t values
// @see: "Mark Jarzynski and Marc Olano, Hash Functions for GPU Rendering, Journal of Computer Graphics Techniques (JCGT), vol. 9, no. 3, 21-38, 2020"
// https://jcgt.org/published/0009/03/02/
inline uint32_t xxhash32(uint32_t lhs, uint32_t rhs)
{
    const uint32_t PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
    const uint32_t PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
    uint32_t h32 = lhs + PRIME32_5 + rhs * PRIME32_3;
    h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
    h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
    h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
    return h32 ^ (h32 >> 16);
}

inline uint32_t hash_combine32(uint32_t lhs, uint32_t rhs)
{
    return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6U) + (lhs >> 2U));
}

template<class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6U) + (seed >> 2U);
}

template<typename It>
std::size_t hash_range(It first, It last)
{
    std::size_t seed = 0;
    for(; first != last; ++first) { hash_combine(seed, *first); }
    return seed;
}

template<class T, class U>
struct pair_hash
{
    inline size_t operator()(const std::pair<T, U> &p) const
    {
        size_t h = 0;
        vierkant::hash_combine(h, p.first);
        vierkant::hash_combine(h, p.second);
        return h;
    }
};

}// namespace vierkant