#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "types.glsl"

layout(push_constant) uniform PushConstants {
    push_constants_t push_constants;
};

layout(std140, binding = 0) uniform UBOMatrices
{
    matrix_struct_t u_matrices[MAX_NUM_DRAWABLES];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_COLOR) in vec4 a_color;
layout(location = ATTRIB_TEX_COORD) in vec2 a_tex_coord;
layout(location = ATTRIB_NORMAL) in vec3 a_normal;
layout(location = ATTRIB_TANGENT) in vec3 a_tangent;

layout(location = 0) out VertexData
{
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec3 eye_vec;
} vertex_out;

void main()
{
    matrix_struct_t m = u_matrices[push_constants.matrix_index + gl_InstanceIndex];

    gl_Position = m.projection * m.modelview * vec4(a_position, 1.0);
    vertex_out.color = a_color;
    vertex_out.tex_coord = (m.texture * vec4(a_tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(m.normal * vec4(a_normal, 1.0)).xyz;
    vertex_out.tangent = normalize(m.normal * vec4(a_tangent, 1.0)).xyz;
    vertex_out.eye_vec = (m.modelview * vec4(a_position, 1.0)).xyz;
}