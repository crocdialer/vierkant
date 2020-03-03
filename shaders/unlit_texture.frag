#version 460
#extension GL_ARB_separate_shader_objects : enable

struct push_constants_t
{
    int drawable_index;
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
    material_struct_t materials[4096];
};

layout(binding = 2) uniform sampler2D m_sampler_2D[1];

layout(location = 0) in VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex_color = texture(m_sampler_2D[0], vertex_in.tex_coord);
    out_color = tex_color * materials[push_constants.drawable_index].color * vertex_in.color;
}