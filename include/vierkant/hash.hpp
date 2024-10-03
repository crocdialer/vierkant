//
// Created by crocdialer on 05.02.23.
//

#pragma once

#include <functional>
#include <cstring>
#include <cstdint>

namespace vierkant
{

//! stripped-out finalizer from murmur3_32
inline uint32_t murmur3_fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

//! stripped-out finalizer from murmur3_64
inline uint64_t murmur3_fmix64(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdLLU;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53LLU;
    k ^= k >> 33;
    return k;
}

// https://en.wikipedia.org/wiki/MurmurHash
inline uint32_t murmur_32_scramble(uint32_t k)
{
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

// https://en.wikipedia.org/wiki/MurmurHash
static inline uint32_t murmur3_32(const uint8_t *key, size_t len, uint32_t seed)
{
    uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for(size_t i = len >> 2; i; i--)
    {
        // Here is a source of differing results across endiannesses.
        // A swap here has no effects on hash properties though.
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for(size_t i = len & 3; i; i--)
    {
        k <<= 8;
        k |= key[i - 1];
    }
    // A swap is *not* necessary here because the preceding loop already
    // places the low bytes in the low places according to whatever endianness
    // we use. Swaps only apply when the memory is copied in a chunk.
    h ^= murmur_32_scramble(k);
    /* Finalize. */
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

// https://en.wikipedia.org/wiki/MurmurHash
template<typename K>
static inline uint32_t murmur3_32(const K &key, uint32_t seed)
{
    constexpr uint32_t num_hashes = sizeof(K) / sizeof(uint32_t);
    constexpr uint32_t num_excess_bytes = sizeof(K) % sizeof(uint32_t);

    uint32_t h = seed;

    if constexpr(num_hashes)
    {
        auto ptr = reinterpret_cast<const uint32_t *>(&key);

        for(uint32_t i = num_hashes; i; i--)
        {
            h ^= murmur_32_scramble(ptr[i - 1]);
            h = (h << 13) | (h >> 19);
            h = h * 5 + 0xe6546b64;
        }
    }

    if constexpr(num_excess_bytes)
    {
        auto end_u8 = reinterpret_cast<const uint8_t *>(&key) + sizeof(uint32_t) * num_hashes;
        uint32_t k = 0;
        for(uint32_t i = num_excess_bytes; i; i--)
        {
            k <<= 8;
            k |= end_u8[i - 1];
        }
        h ^= murmur_32_scramble(k);
    }

    // finalize
    h ^= sizeof(K);
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
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