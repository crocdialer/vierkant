#ifndef UTILS_HASH_GLSL
#define UTILS_HASH_GLSL

uint simple_hash(uint a)
{
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return a;
}

//! stripped-out finalizer from murmur3_32
uint murmur3_fmix32(uint h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

uint murmur_32_scramble(uint k)
{
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

//! condensed version of murmur3_32 for uint32_t
uint murmur3_32(const uint key, uint seed)
{
    uint len = 4;// sizeof(key_t)
    uint h = seed;
    h ^= murmur_32_scramble(key);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;

    /* Finalize. */
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

uint hash_combine32(uint lhs, uint rhs)
{
    return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6U) + (lhs >> 2U));
}

#endif // UTILS_HASH_GLSL