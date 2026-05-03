module phase_function;

import "sampling.slang";
import "constants.slang";

namespace utils
{

//! Henyey-Greenstein phase function
public float phase_hg(float cos_theta, float g)
{
    float denom = 1 + g * g - 2 * g * cos_theta;
    return ONE_OVER_FOUR_PI * (1 - g * g) / (denom * sqrt(denom));
}

//! return a sample from Henyey-Greenstein phase-function
public float3 sample_phase_hg(float2 Xi, float g, out float pdf)
{
    float phi = Xi.y * 2 * PI;
    float cos_theta;
    if (abs(g) < 1e-3) { cos_theta = 1 - 2 * Xi.x; }
    else
    {
        float sqr_term = (1 - g * g) / (1 - g + 2 * g * Xi.x);
        cos_theta = (1 + g * g - sqr_term * sqr_term) / (2 * g);
    }
    float sin_theta = clamp(sqrt(1.0 - (cos_theta * cos_theta)), 0.0, 1.0);
    pdf = phase_hg(cos_theta, g);
    return spherical_direction(sin_theta, cos_theta, phi);
}

} // namespace utils
