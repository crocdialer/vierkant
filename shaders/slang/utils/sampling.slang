module sampling;

import "constants.slang";

namespace utils
{

//! return a Hammersley point in range [0, 1]
public float2 Hammersley(uint i, uint N)
{
    float vdc = float(reversebits(i)) * 2.3283064365386963e-10; // Van der Corput
    return float2(float(i) / float(N), vdc);
}

//! returns a local coordinate frame for a given normalized direction
public float3x3 local_frame(float3 direction)
{
    float len2 = dot(direction.xy, direction.xy);
    float3 tangentX = len2 > 0 ? float3(-direction.y, direction.x, 0) / sqrt(len2) : float3(1, 0, 0);
    float3 tangentY = cross(direction, tangentX);
    return float3x3(tangentX, tangentY, direction);
}

//! helper function, return a vector for spherical-angles theta/phi
public float3 spherical_direction(float sin_theta, float cos_theta, float phi)
{
    return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

//! sample uniformly distributed points on a unit-disc
public float2 sample_unit_disc(float2 Xi)
{
    // done to achieve uniform distribution on disc
    float r = sqrt(Xi.x);

    // [0, 2PI]
    float theta = 2.0 * utils::PI * Xi.y;

    return float2(r * cos(theta), r * sin(theta));
}

//! sample a unit-sphere
public float3 sample_unit_sphere(float2 Xi)
{
    // [0, 2PI]
    float phi = 2.0 * utils::PI * Xi.y;

    // [-1, 1]
    float cos_theta = 2.0 * Xi.x - 1.0;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a spherical cap defined by angle sigma
public float3 sample_unit_sphere_cap(float2 Xi, float sigma)
{
    float phi = 2.0 * utils::PI * Xi.y;

    // [cos(sigma), 1]
    float cos_sigma = cos(sigma);
    float cos_theta = Xi.x * (1 - cos_sigma) + cos_sigma;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a unit-hemisphere
public float3 sample_hemisphere_uniform(float2 Xi)
{
    // [0, 2PI]
    float phi = 2.0 * utils::PI * Xi.y;

    // [0, 1]
    float cos_theta = Xi.x;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a cosine-weighted hemisphere-distribution
public float3 sample_hemisphere_cosine(float2 Xi)
{
    float phi = 2.0 * utils::PI * Xi.y;
    float cos_theta = sqrt(max(1.0 - Xi.x, 0.0));
    float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a GGX-distribution
public float3 sample_GGX(float2 Xi, float roughness)
{
    float a = max(0.001, roughness);
    float phi = 2.0 * utils::PI * Xi.x;
    float cos_theta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
    float sin_theta = sqrt(clamp(1.0 - cos_theta * cos_theta, 0.0, 1.0));
    return spherical_direction(sin_theta, cos_theta, phi);
}

// sample a 'GGX-distribution of visible normals' (from Eric Heitz, 2018)
public float3 sample_GGX_VNDF(float2 Xi, float3 wi, float2 alpha)
{
    // warp to the hemisphere configuration
    float3 wiStd = normalize(float3(wi.xy * alpha, wi.z));

    // sample a spherical cap in (-wi.z, 1]
    float phi = (2.0 * Xi.x - 1.0) * utils::PI;

    float z = fma((1.0 - Xi.y), (1.0 + wiStd.z), -wiStd.z);
    float sinTheta = sqrt(clamp(1.0 - z * z, 0.0, 1.0));
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float3 c = float3(x, y, z);

    // compute halfway direction as standard normal
    float3 wmStd = c + wiStd;

    // warp back to the ellipsoid configuration
    float3 wm = normalize(float3(wmStd.xy * alpha, wmStd.z));
    return wm;
}

public float3 sample_GTR1(float2 Xi, float roughness)
{
    float a = max(0.001, roughness);
    float a2 = a * a;

    float phi = Xi.y * 2 * utils::PI;
    float cos_theta = sqrt((1.0 - pow(a2, 1.0 - Xi.x)) / (1.0 - a2));
    float sin_theta = clamp(sqrt(1.0 - (cos_theta * cos_theta)), 0.0, 1.0);
    return spherical_direction(sin_theta, cos_theta, phi);
}

}// namespace utils
