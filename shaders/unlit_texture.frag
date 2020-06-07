#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "types.glsl"

#define MAX_NUM_DRAWABLES 4096

layout(push_constant) uniform PushConstants {
    push_constants_t push_constants;
};

layout(std140, binding = 1) uniform ubo_materials
{
    material_struct_t materials[MAX_NUM_DRAWABLES];
};

layout(binding = 2) uniform sampler2D u_sampler_2D[1];

layout(location = 0) in VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex_color = texture(u_sampler_2D[0], vertex_in.tex_coord);
    out_color = tex_color * materials[push_constants.material_index].color * vertex_in.color;
}