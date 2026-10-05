module color_ST2084_PQ;

namespace utils
{

// ST 2084 Perceptual Quantizer (PQ) EOTF and inverse EOTF
// see SMPTE ST 2084:2014
// linear values are normalized, 1.0 <-> 10000 cd/m²

static const float c1 = 0.8359375;      // 3424/4096
static const float c2 = 18.8515625;     // 2413/128
static const float c3 = 18.6875;        // 2392/128
static const float m1 = 0.1593017578125;// 2610/16384
static const float m2 = 78.84375;       // 2523/32

//! PQ code-value -> normalized linear luminance
public float3 color_st2084_pq_eotf(float3 value)
{
    float3 value_m2 = pow(value, 1.0 / m2);
    float3 num = max(value_m2 - c1, float3(0.0));
    float3 denom = c2 - c3 * value_m2;
    return pow(num / denom, 1.0 / m1);
}

//! normalized linear luminance -> PQ code-value
public float3 color_st2084_pq_inverse_eotf(float3 value)
{
    float3 value_m1 = pow(value, m1);
    float3 num = c1 + c2 * value_m1;
    float3 denom = 1.0 + c3 * value_m1;
    return pow(num / denom, m2);
}

} // namespace utils
