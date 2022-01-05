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

layout(std140, binding = BINDING_BONES) uniform UBOBones
{
    mat4 u_bones[MAX_NUM_BONES];
};

layout(std140, binding = BINDING_PREVIOUS_BONES) uniform UBOPreviousBones
{
    mat4 u_previous_bones[MAX_NUM_BONES];
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
layout(location = ATTRIB_BONE_INDICES) in uvec4 a_bone_ids;
layout(location = ATTRIB_BONE_WEIGHTS) in vec4 a_bone_weights;

layout(location = 0) out VertexData
{
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
} vertex_out;

void main()
{
    matrix_struct_t m = u_matrices[context.matrix_index + gl_InstanceIndex];
    matrix_struct_t m_last = u_previous_matrices[context.matrix_index + gl_InstanceIndex];

    vec4 current_vertex = vec4(0);
    vec4 last_vertex = vec4(0);

    for (int i = 0; i < 4; i++)
    {
        current_vertex += u_bones[a_bone_ids[i]] * vec4(a_position, 1.0) * a_bone_weights[i];
        last_vertex += u_previous_bones[a_bone_ids[i]] * vec4(a_position, 1.0) * a_bone_weights[i];
    }
    vertex_out.color = a_color;
    vertex_out.tex_coord = (m.texture * vec4(a_tex_coord, 0, 1)).xy;
    vertex_out.normal = normalize(m.normal * vec4(a_normal, 1.0)).xyz;
    vertex_out.tangent = normalize(m.normal * vec4(a_tangent, 1.0)).xyz;

    vertex_out.current_position = m.projection * m.modelview * vec4(current_vertex.xyz, 1.0);
    vertex_out.last_position = m_last.projection * m_last.modelview * vec4(last_vertex.xyz, 1.0);

    gl_Position = vertex_out.current_position;
}