#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"
#include "../utils/camera.glsl"

layout (vertices = 3) out;

struct VertexData
{
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
};
layout(location = 0) flat in uint in_object_index[];
layout(location = 1) in VertexData vertex_in[];

layout(location = 0) flat out uint out_object_index[3];
layout(location = 1) out VertexData vertex_out[3];

void main(void)
{
    if (gl_InvocationID == 0)
    {
        gl_TessLevelInner[0] = 1.0;
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    out_object_index[gl_InvocationID] = in_object_index[gl_InvocationID];
    vertex_out[gl_InvocationID] = vertex_in[gl_InvocationID];
}