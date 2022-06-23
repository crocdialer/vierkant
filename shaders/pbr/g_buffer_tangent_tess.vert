#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_TEX_COORD) in vec2 a_tex_coord;
layout(location = ATTRIB_NORMAL) in vec3 a_normal;
layout(location = ATTRIB_TANGENT) in vec3 a_tangent;

layout(location = 0) flat out uint object_index;
layout(location = 1) out VertexData
{
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    object_index = gl_InstanceIndex;

    vertex_out.current_position = vec4(a_position, 1.0);
    vertex_out.last_position = vec4(a_position, 1.0);

    gl_Position = vertex_out.current_position;

    vertex_out.tex_coord = a_tex_coord;
    vertex_out.normal = a_normal;
    vertex_out.tangent = a_tangent;
}