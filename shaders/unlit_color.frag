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


struct material_struct_t
{
    vec4 color;
    vec4 emission;
    float metalness;
    float roughness;
};

layout(std140, binding = 1) uniform ubo_materials
{
    material_struct_t materials[MAX_NUM_DRAWABLES];
};

layout(location = 0) in VertexData
{
    vec4 color;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vertex_in.color * materials[push_constants.material_index].color;
}