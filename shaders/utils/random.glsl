#ifndef UTILS_RANDOM_GLSL
#define UTILS_RANDOM_GLSL

#extension GL_EXT_control_flow_attributes : require

// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
uint tea(uint lhs, uint rhs)
{
    uint v0 = lhs;
    uint v1 = rhs;
    uint s0 = 0;

    [[unroll]]
    for (uint n = 0; n < 16; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

// Generate a random unsigned int from two unsigned int values
// @see: "Mark Jarzynski and Marc Olano, Hash Functions for GPU Rendering, Journal of Computer Graphics Techniques (JCGT), vol. 9, no. 3, 21-38, 2020"
// https://jcgt.org/published/0009/03/02/
uint xxhash32(uint lhs, uint rhs)
{
    const uint PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
    const uint PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
    uint h32 = lhs + PRIME32_5 + rhs * PRIME32_3;
    h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
    h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
    h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
    return h32 ^ (h32 >> 16);
}

uint hash(uint a)
{
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return a;
}

////! random number generation using pcg32i_random_t, using inc = 1. Our random state is a uint.
//uint rng_step(uint rng_state)
//{
//    return rng_state * 747796405 + 1;
//}
//
////! steps the RNG and returns a floating-point value between 0 and 1 inclusive.
//float rng_float(inout uint rng_state)
//{
//    // condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
//    rng_state  = rng_step(rng_state);
//    uint word = ((rng_state >> ((rng_state >> 28) + 4)) ^ rng_state) * 277803737;
//    word      = (word >> 22) ^ word;
//    return float(word) / 4294967295.0f;
//}

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
    uint LCG_A = 1664525u;
    uint LCG_C = 1013904223u;
    prev       = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
    return (float(lcg(prev)) / float(0x01000000));
}
    
#endif // UTILS_RANDOM_GLSL