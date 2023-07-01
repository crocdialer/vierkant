#ifndef UTILS_SAMPLING_GLSL
#define UTILS_SAMPLING_GLSL

#define PI 3.1415926535897932384626433832795
#define ONE_OVER_PI 0.31830988618379067153776752674503

//! returns a local coordinate frame for a given normalized direction
mat3 local_frame(in vec3 direction)
{
    float len2 = dot(direction.xy, direction.xy);
    vec3 tangentX = len2 > 0 ? vec3(-direction.y, direction.x, 0) / sqrt(len2) : vec3(1, 0, 0);
    vec3 tangentY = cross(direction, tangentX);
    return mat3(tangentX, tangentY, direction);
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

//! random point on a unit-sphere
vec3 sample_unit_sphere(vec2 Xi)
{
    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    // [-1, 1]
    float u = 2.0 * Xi.x - 1.0;

    const float r = sqrt(1.0 - u * u);
    return vec3(r * cos(theta), r * sin(theta), u);
}

//! random point on a spherical cap defined by angle sigma
vec3 sample_unit_sphere_cap(vec2 Xi, float sigma)
{
    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    // [cos(sigma), 1]
    float cos_sigma = cos(sigma);
    float u = Xi.x * (1 - cos_sigma) + cos_sigma;

    const float r = sqrt(1.0 - u * u);
    return vec3(r * cos(theta), r * sin(theta), u);
}

//! random point on a unit-hemisphere
vec3 sample_hemisphere_uniform(vec2 Xi)
{
    // [0, 2pi]
    const float theta = 2.0 * PI * Xi.y;

    // [0, 1]
    float u = Xi.x;

    const float r = sqrt(1.0 - u * u);
    return vec3(r * cos(theta), r * sin(theta), u);
}

//! sample a cosine-weighted hemisphere-distribution
vec3 sample_hemisphere_cosine(vec2 Xi)
{
    float cosTheta = sqrt(max(1.0 - Xi.y, 0.0));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float phi = 2.0 * PI * Xi.x;

    // L
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

//! sample a GGX-distribution
vec3 sample_GGX(vec2 Xi, float roughness)
{
    float a = max(0.001, roughness);

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
    float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));

    // H
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// sample a 'GGX-distribution of visible normals' (from Eric Heitz, 2018)
vec3 sample_GGX_VNDF(vec2 Xi, vec3 V, vec2 roughness)
{
    roughness = max(vec2(0.001), roughness);

    // transform view-direction to hemisphere configuration
    vec3 Vh = normalize(vec3(roughness * V.xy, V.z));

    // orthonormal basis
    mat3 basis = local_frame(Vh);

    // parametrization of projected area
    float phi = 2.0 * PI * Xi.x;
    float r = sqrt(Xi.y);

    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    // reprojection onto hemisphere
    vec3 Nh = t1 * basis[0] + t2 * basis[1] + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    // transforming normal back to ellipsoid configuration
    vec3 Ne = normalize(vec3(roughness * Nh.xy, max(0.0, Nh.z)));
    return Ne;
}

vec3 sample_GTR1(vec2 Xi, float roughness)
{
    float a = max(0.001, roughness);
    float a2 = a * a;

    float phi = Xi.x * 2 * PI;
    float cosTheta = sqrt((1.0 - pow(a2, 1.0 - Xi.y)) / (1.0 - a2));
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

#endif // UTILS_SAMPLING_GLSL