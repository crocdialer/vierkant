#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = BINDING_MATRIX) uniform UBOMatrices
{
    matrix_struct_t u_matrices[MAX_NUM_DRAWABLES];
};

layout(std140, binding = BINDING_PREVIOUS_MATRIX) uniform UBOPreviousMatrices
{
    matrix_struct_t u_previous_matrices[MAX_NUM_DRAWABLES];
};

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    vec2 u_jitter_offset;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_COLOR) in vec4 a_color;
layout(location = ATTRIB_TEX_COORD) in vec2 a_tex_coord;
layout(location = ATTRIB_NORMAL) in vec3 a_normal;

layout(location = 0) out VertexData
{
    vec4 color;
    vec3 normal;

    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    matrix_struct_t m = u_matrices[context.matrix_index + gl_InstanceIndex];
    matrix_struct_t m_last = u_previous_matrices[context.matrix_index + gl_InstanceIndex];

    vertex_out.current_position = m.projection * m.modelview * vec4(a_position, 1.0);
    vertex_out.last_position = m_last.projection * m_last.modelview * vec4(a_position, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += u_jitter_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.color = a_color;
    vertex_out.normal = normalize(m.normal * vec4(a_normal, 1.0)).xyz;
}