#version 460
#extension GL_ARB_separate_shader_objects : enable

#define MAX_NUM_DRAWABLES 4096

struct push_constants_t
{
    int matrix_index;
    int material_index;
};

layout(push_constant) uniform PushConstants {
    push_constants_t push_constants;
};

struct matrix_struct_t
{
    mat4 modelview;
    mat4 projection;
    mat4 normal;
    mat4 texture;
};

layout(std140, binding = 0) uniform UBOMatrices
{
    matrix_struct_t matrices[MAX_NUM_DRAWABLES];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;

layout(location = 0) out VertexData
{
    vec4 color;
} vertex_out;

void main()
{
    matrix_struct_t m = matrices[push_constants.matrix_index + gl_InstanceIndex];
    gl_Position = m.projection * m.modelview * vec4(a_position, 1.0);
    vertex_out.color = a_color;
}