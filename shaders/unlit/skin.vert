#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std430, set = 0, binding = BINDING_MESH_DRAWS) readonly buffer MeshDrawBuffer
{
    mesh_draw_t draws[];
};

layout(std140, binding = BINDING_BONES) readonly buffer UBOBones
{
    transform_t u_bones[];
};

layout(location = ATTRIB_POSITION) in vec3 a_position;
layout(location = ATTRIB_COLOR) in vec4 a_color;
layout(location = ATTRIB_TEX_COORD) in vec2 a_tex_coord;
layout(location = ATTRIB_NORMAL) in vec3 a_normal;
layout(location = ATTRIB_TANGENT) in vec3 a_tangent;
layout(location = ATTRIB_BONE_INDICES) in ivec4 a_bone_ids;
layout(location = ATTRIB_BONE_WEIGHTS) in vec4 a_bone_weights;

layout(location = 0) out VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_out;

void main()
{
    uint object_index = gl_InstanceIndex;//gl_BaseInstance + gl_InstanceIndex
    matrix_struct_t m = draws[object_index].current_matrices;

    vec3 new_vertex = vec3(0);

    for (int i = 0; i < 4; i++)
    {
        new_vertex += apply_transform(u_bones[a_bone_ids[i]], a_position) * a_bone_weights[i];
    }
    vertex_out.color = a_color;
    vertex_out.tex_coord = (m.texture * vec4(a_tex_coord, 0, 1)).xy;
    gl_Position = m.projection * vec4(apply_transform(m.transform, new_vertex), 1.0);
}