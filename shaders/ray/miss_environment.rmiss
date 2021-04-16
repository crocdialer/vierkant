#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

layout(binding = 11) uniform samplerCube u_sampler_cube;

layout(location = 0) rayPayloadInEXT payload_t payload;

void main()
{
    // stop path tracing loop from rgen shader
    payload.stop = true;
    payload.normal = vec3(0.);
    payload.position = vec3(0.);
    payload.radiance += payload.beta * texture(u_sampler_cube, gl_WorldRayDirectionEXT).rgb;
}