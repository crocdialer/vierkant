#ifndef COLOR_ST2084_PQ_GLSL
#define COLOR_ST2084_PQ_GLSL

// ST 2084 Perceptual Quantizer (PQ) EOTF and inverse EOTF
// see SMPTE ST 2084:2014

vec3 color_st2084_pq_eotf(vec3 value)
{
    const vec3 c1 = vec3(0.8359375, 0.8359375, 0.8359375); // 3424/4096
    const vec3 c2 = vec3(18.8515625, 18.8515625, 18.8515625); // 2413/128
    const vec3 c3 = vec3(18.6875, 18.6875, 18.6875); // 2392/128
    const vec3 m1 = vec3(0.1593017578125, 0.1593017578125, 0.1593017578125); // 2610/16384
    const vec3 m2 = vec3(78.84375, 78.84375, 78.84375); // 2523/32

    vec3 value_m1 = pow(value, m1);
    vec3 num = max(value_m1 - c1, vec3(0.0));
    vec3 denom = c2 - c3 * value_m1;
    vec3 result = pow(num / denom, m2);
    return result;
}

vec3 color_st2084_pq_inverse_eotf(vec3 value)
{
    const vec3 c1 = vec3(0.8359375, 0.8359375, 0.8359375); // 3424/4096
    const vec3 c2 = vec3(18.8515625, 18.8515625, 18.8515625); // 2413/128
    const vec3 c3 = vec3(18.6875, 18.6875, 18.6875); // 2392/128
    const vec3 m1 = vec3(0.1593017578125, 0.1593017578125, 0.1593017578125); // 2610/16384
    const vec3 m2 = vec3(78.84375, 78.84375, 78.84375); // 2523/32

    vec3 value_m2 = pow(value, 1.0 / m2);
    vec3 num = c1 + c2 * value_m2;
    vec3 denom = 1.0 + c3 * value_m2;
    vec3 result = pow(num / denom, 1.0 / m1);
    return result;
}

#endif // COLOR_ST2084_PQ_GLSL