#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

#define PI 3.1415926535897932384626433832795

layout(binding = 11) uniform samplerCube u_sampler_cube;

layout(location = 0) rayPayloadInEXT payload_t payload;

const float eps =  0.001;

float powerHeuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

void main()
{
//    float cube_width = float(textureSize(u_sampler_cube, 0).x);
//    float solid_angle_texel = 4.0 * PI / (6.0 * cube_width * cube_width);
//    float solid_angle_sample = solid_angle_texel / payload.pdf + EPS;
//    float lod = 0.5 * log2(solid_angle_sample / solid_angle_texel);
//    float lod = 0.5 * log2(1.0 / (payload.pdf + EPS));

    float lod = log2(abs(payload.cone.width));

    // stop path tracing loop from rgen shader
    payload.stop = true;

    payload.radiance += payload.beta * textureLod(u_sampler_cube, gl_WorldRayDirectionEXT, lod).rgb;
}