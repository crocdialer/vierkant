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