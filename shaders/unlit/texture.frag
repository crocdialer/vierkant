#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MATERIAL) readonly buffer MaterialBuffer
{
    material_struct_t materials[];
};

layout(binding = BINDING_TEXTURES) uniform sampler2D u_sampler_2D[1];

layout(location = 0) in VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex_color = texture(u_sampler_2D[0], vertex_in.tex_coord);
    out_color = tex_color * materials[context.object_index].color * vertex_in.color;
}