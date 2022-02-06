#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};


layout(std140, set = 0, binding = BINDING_MATRIX) readonly buffer MatrixBuffer
{
    matrix_struct_t matrices[];
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_COLOR) in vec4 a_color;

layout(location = 0) out VertexData
{
    vec4 color;
} vertex_out;

void main()
{
    matrix_struct_t m = matrices[gl_BaseInstance + gl_InstanceIndex];
    gl_Position = m.projection * m.modelview * vec4(a_position, 1.0);
    vertex_out.color = a_color;
}