#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"
#include "../utils/sdf.glsl"

layout(location = 0) rayPayloadInEXT shadow_payload_t payload;

layout(std140, binding = 11) uniform ubo_t
{
    float environment_factor;
} ubo;

void main()
{
    payload.shadow = false;
}