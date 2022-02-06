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

layout(std140, set = 0, binding = BINDING_PREVIOUS_MATRIX) readonly buffer MatrixBufferPrevious
{
    matrix_struct_t u_previous_matrices[];
};

layout(std140, binding = BINDING_JITTER_OFFSET) uniform UBOJitter
{
    vec2 u_jitter_offset;
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
    uint object_index = context.object_index;//gl_BaseInstance + gl_InstanceIndex;
    matrix_struct_t m = u_matrices[object_index];
    matrix_struct_t m_last = u_previous_matrices[object_index];

    vertex_out.current_position = m.projection * m.modelview * vec4(a_position, 1.0);
    vertex_out.last_position = m_last.projection * m_last.modelview * vec4(a_position, 1.0);

    vec4 jittered_position = vertex_out.current_position;
    jittered_position.xy += 2.0 * u_jitter_offset * jittered_position.w;
    gl_Position = jittered_position;

    vertex_out.color = a_color;
    vertex_out.normal = normalize(m.normal * vec4(a_normal, 1.0)).xyz;
}