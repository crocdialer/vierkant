#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference2: require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

layout(binding = 1, set = 0) uniform TraceData
{
    trace_data_t trace_data;
};

layout(binding = 3) uniform samplerCube u_sampler_cube;

layout(location = 0) rayPayloadInEXT payload_t payload;

void main()
{
    vec2 sz = textureSize(u_sampler_cube, 0);
    float lod = 0.5 * log2(sz.x * sz.y);

    lod += log2(abs(payload.cone.width));

    // stop path tracing loop from rgen shader
    payload.stop = true;

    payload.radiance += trace_data.params.environment * payload.beta *
                            textureLod(u_sampler_cube, gl_WorldRayDirectionEXT, lod).rgb;
}