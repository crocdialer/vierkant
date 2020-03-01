#version 460
#extension GL_ARB_separate_shader_objects : enable

struct material_struct_t
{
    vec4 color;
    vec4 emission;
    float metalness;
    float roughness;
};

layout(std140, binding = 1) uniform ubo_materials
{
    material_struct_t materials[1];
};

layout(location = 0) in VertexData
{
    vec4 color;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vertex_in.color * materials[0].color;
}