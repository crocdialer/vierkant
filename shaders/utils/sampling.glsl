#ifndef UTILS_SAMPLING_GLSL
#define UTILS_SAMPLING_GLSL

#include "constants.glsl"

//! return a Hammersley point in range [0, 1]
vec2 Hammersley(uint i, uint N)
{
    float vdc = float(bitfieldReverse(i)) * 2.3283064365386963e-10; // Van der Corput
    return vec2(float(i) / float(N), vdc);
}

//! returns a local coordinate frame for a given normalized direction
mat3 local_frame(in vec3 direction)
{
    float len2 = dot(direction.xy, direction.xy);
    vec3 tangentX = len2 > 0 ? vec3(-direction.y, direction.x, 0) / sqrt(len2) : vec3(1, 0, 0);
    vec3 tangentY = cross(direction, tangentX);
    return mat3(tangentX, tangentY, direction);
}

//! helper function, return a vector for spherical-angles theta/phi
vec3 spherical_direction(float sin_theta, float cos_theta, float phi)
{
    return vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

//! sample uniformly distributed points on a unit-disc
vec2 sample_unit_disc(vec2 Xi)
{
    // done to achieve uniform distribution on disc
    float r = sqrt(Xi.x);

    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    return vec2(r * cos(theta), r * sin(theta));
}

//! sample a unit-sphere
vec3 sample_unit_sphere(vec2 Xi)
{
    // [0, 2pi]
    float phi = 2.0 * PI * Xi.y;

    // [-1, 1]
    float cos_theta = 2.0 * Xi.x - 1.0;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a spherical cap defined by angle sigma
vec3 sample_unit_sphere_cap(vec2 Xi, float sigma)
{
    float phi = 2.0 * PI * Xi.y;

    // [cos(sigma), 1]
    float cos_sigma = cos(sigma);
    float cos_theta = Xi.x * (1 - cos_sigma) + cos_sigma;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a unit-hemisphere
vec3 sample_hemisphere_uniform(vec2 Xi)
{
    // [0, 2pi]
    float phi = 2.0 * PI * Xi.y;

    // [0, 1]
    float cos_theta = Xi.x;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a cosine-weighted hemisphere-distribution
vec3 sample_hemisphere_cosine(vec2 Xi)
{
    float phi = 2.0 * PI * Xi.y;
    float cos_theta = sqrt(max(1.0 - Xi.x, 0.0));
    float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));
    return spherical_direction(sin_theta, cos_theta, phi);
}

//! sample a GGX-distribution
vec3 sample_GGX(vec2 Xi, float roughness)
{
    float a = max(0.001, roughness);
    float phi = 2.0 * PI * Xi.x;
    float cos_theta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
    float sin_theta = sqrt(clamp(1.0 - cos_theta * cos_theta, 0.0, 1.0));
    return spherical_direction(sin_theta, cos_theta, phi);
}

// sample a 'GGX-distribution of visible normals' (from Eric Heitz, 2018)
vec3 sample_GGX_VNDF(vec2 Xi, vec3 wi, vec2 alpha)
{
    // warp to the hemisphere configuration
    vec3 wiStd = normalize(vec3(wi.xy * alpha, wi.z));

    // sample a spherical cap in (-wi.z, 1]
    float phi = (2.0f * Xi.x - 1.0f) * PI;

    float z = fma((1.0f - Xi.y), (1.0f + wiStd.z), -wiStd.z);
    float sinTheta = sqrt(clamp(1.0f - z * z, 0.0f, 1.0f));
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    vec3 c = vec3(x, y, z);

    // compute halfway direction as standard normal
    vec3 wmStd = c + wiStd;

    // warp back to the ellipsoid configuration
    vec3 wm = normalize(vec3(wmStd.xy * alpha, wmStd.z));
    return wm;
}

vec3 sample_GTR1(vec2 Xi, float roughness)
{
    float a = max(0.001, roughness);
    float a2 = a * a;

    float phi = Xi.y * 2 * PI;
    float cos_theta = sqrt((1.0 - pow(a2, 1.0 - Xi.x)) / (1.0 - a2));
    float sin_theta = clamp(sqrt(1.0 - (cos_theta * cos_theta)), 0.0, 1.0);
    return spherical_direction(sin_theta, cos_theta, phi);
}

#endif // UTILS_SAMPLING_GLSL