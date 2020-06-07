#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "types.glsl"

layout(push_constant) uniform PushConstants {
    push_constants_t push_constants;
};

layout(std140, binding = 0) uniform UBOMatrices
{
    matrix_struct_t matrices[MAX_NUM_DRAWABLES];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_COLOR) in vec4 a_color;

layout(location = 0) out VertexData
{
    vec3 eye_vec;
} vertex_out;

void main()
{
    matrix_struct_t m = matrices[push_constants.matrix_index + gl_InstanceIndex];
    vertex_out.eye_vec = a_position.xyz;
    gl_Position = m.projection * m.modelview * vec4(a_position, 1.0);
}