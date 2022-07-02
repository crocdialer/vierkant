#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../renderer/types.glsl"

//! Vertex defines the layout for a vertex-struct
struct Vertex
{
    vec3 position;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
};

layout(set = 0, binding = BINDING_VERTICES, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

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
    Vertex v = vertices[gl_VertexIndex];

    object_index = gl_BaseInstance;

    vertex_out.current_position = vec4(v.position, 1.0);
    vertex_out.last_position = vec4(v.position, 1.0);

    gl_Position = vertex_out.current_position;

    vertex_out.tex_coord = v.tex_coord;
    vertex_out.normal = v.normal;
    vertex_out.tangent = v.tangent;
}