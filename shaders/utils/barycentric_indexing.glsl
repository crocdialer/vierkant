#ifndef UTILS_BARYCENTRIC_INDEXING_GLSL
#define UTILS_BARYCENTRIC_INDEXING_GLSL

//! number of micro-triangles for a given subdivision-level
uint num_micro_triangles(uint num_levels) { return 1 << (num_levels << 1u); }

//! even bits
uint extract_even_bits(uint x)
{
    x &= 0x55555555u;
    x = (x | (x >> 1)) & 0x33333333u;
    x = (x | (x >> 2)) & 0x0f0f0f0fu;
    x = (x | (x >> 4)) & 0x00ff00ffu;
    x = (x | (x >> 8)) & 0x0000ffffu;
    return x;
}

//! exclusive prefix or (log(n) XOR's and SHF's)
uint prefixEor(uint x)
{
    x ^= x >> 1;
    x ^= x >> 2;
    x ^= x >> 4;
    x ^= x >> 8;
    return x;
}

//! convert distance along the curve to discrete barycentrics
void index2dbary(uint index, out uint u, out uint v, out uint w)
{
    uint b0 = extract_even_bits(index);
    uint b1 = extract_even_bits(index >> 1u);

    uint fx = prefixEor(b0);
    uint fy = prefixEor(b0 & ~b1);

    uint t = fy ^ b1;

    u = (fx & ~t) | (b0 & ~t) | (~b0 & ~fx & t);
    v = fy ^ b0;
    w = (~fx & ~t) | (b0 & ~t) | (~b0 & fx & t);
}

void index2bary(uint index, uint level, out vec2 uv0, out vec2 uv1, out vec2 uv2)
{
    if(level == 0)
    {
        uv0 = vec2(0, 0);
        uv1 = vec2(1, 0);
        uv2 = vec2(0, 1);
        return;
    }
    uint iu, iv, iw;
    index2dbary(index, iu, iv, iw);

    // consider only "level" bits
    iu = iu & ((1u << level) - 1);
    iv = iv & ((1u << level) - 1);
    iw = iw & ((1u << level) - 1);

    bool upright = bool((iu & 1u) ^ (iv & 1u) ^ (iw & 1u));

    if(!upright)
    {
        iu = iu + 1;
        iv = iv + 1;
    }
    float level_scale = uintBitsToFloat((127u - level) << 23u);

    // scale the barycentic coordinate to the global space/scale
    float du = 1.f * level_scale;
    float dv = 1.f * level_scale;

    // scale the barycentic coordinate to the global space/scale
    float u = iu * level_scale;
    float v = iv * level_scale;

    if(!upright)
    {
        du = -du;
        dv = -dv;
    }
    uv0 = vec2(u, v);
    uv1 = vec2(u + du, v);
    uv2 = vec2(u, v + dv);
}
    
#endif // UTILS_BARYCENTRIC_INDEXING_GLSL