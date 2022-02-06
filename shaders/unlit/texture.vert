#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MATRIX) readonly buffer MatrixBuffer
{
    matrix_struct_t u_matrices[];
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_COLOR) in vec4 a_color;
layout(location = ATTRIB_TEX_COORD) in vec2 a_tex_coord;

layout(location = 0) out VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_out;

void main()
{
    uint object_index = gl_BaseInstance + gl_InstanceIndex;
    matrix_struct_t m = u_matrices[object_index];

    gl_Position = m.projection * m.modelview * vec4(a_position, 1.0);
    vertex_out.color = a_color;
    vertex_out.tex_coord = (m.texture * vec4(a_tex_coord, 0, 1)).xy;
}