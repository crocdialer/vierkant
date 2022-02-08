#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MATERIAL) readonly buffer MaterialBuffer
{
    material_struct_t materials[];
};

layout(set = 1, binding = BINDING_TEXTURES) uniform sampler2D u_sampler_2D[];

layout(location = 0) flat in uint object_index;
layout(location = 1) in VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex_color = texture(u_sampler_2D[materials[object_index].base_texture_index], vertex_in.tex_coord);
    out_color = tex_color * materials[object_index].color * vertex_in.color;
}