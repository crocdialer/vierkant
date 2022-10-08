#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

layout(location = 0) rayPayloadInEXT shadow_payload_t payload;

void main()
{
    payload.shadow = false;
}