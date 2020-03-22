#version 460
#extension GL_ARB_separate_shader_objects : enable

#define MAX_NUM_DRAWABLES 4096
#define MAX_NUM_BONES 512

struct matrix_struct_t
{
    mat4 modelview;
    mat4 projection;
    mat4 normal;
    mat4 texture;
};

struct push_constants_t
{
    int matrix_index;
    int material_index;
};

layout(push_constant) uniform PushConstants {
    push_constants_t push_constants;
};

layout(std140, binding = 0) uniform UBOMatrices
{
    matrix_struct_t u_matrices[MAX_NUM_DRAWABLES];
};

layout(std140, binding = 3) uniform UBOBones
{
    mat4 u_bones[MAX_NUM_BONES];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_tex_coord;
layout(location = 3) in vec3 a_normals;
layout(location = 4) in vec3 a_tangents;
layout(location = 5) in ivec4 a_bone_ids;
layout(location = 6) in vec4 a_bone_weights;

layout(location = 0) out VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_out;

void main()
{
    matrix_struct_t m = u_matrices[push_constants.matrix_index + gl_InstanceIndex];

    vec4 new_vertex = vec4(0);

    for (int i = 0; i < 4; i++)
    {
        new_vertex += u_bones[a_bone_ids[i]] * vec4(a_position, 1.0) * a_bone_weights[i];
    }

    gl_Position = m.projection * m.modelview * vec4(new_vertex.xyz, 1.0);
    vertex_out.color = a_color;
    vertex_out.tex_coord = (m.texture * vec4(a_tex_coord, 0, 1)).xy;
}