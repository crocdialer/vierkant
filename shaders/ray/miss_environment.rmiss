#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

#define PI 3.1415926535897932384626433832795

layout(binding = 11) uniform samplerCube u_sampler_cube;

layout(location = 0) rayPayloadInEXT payload_t payload;

const float eps =  0.001;

void main()
{
    float cube_width = float(textureSize(u_sampler_cube, 0).x);
    float solid_angle_texel = 4.0 * PI / (6.0 * cube_width * cube_width);
    float solid_angle_sample = 4.0 * PI * (1.0 - payload.pdf);
    float lod = 0.5 * log2(solid_angle_sample / solid_angle_texel);

    // stop path tracing loop from rgen shader
    payload.stop = true;
//    payload.normal = vec3(0);
//    payload.position = vec3(0);

    payload.radiance += payload.beta * textureLod(u_sampler_cube, gl_WorldRayDirectionEXT, lod).rgb;
//    payload.radiance += payload.beta * texture(u_sampler_cube, gl_WorldRayDirectionEXT).rgb;
}