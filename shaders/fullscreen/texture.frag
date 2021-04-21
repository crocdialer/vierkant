#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../utils/tonemap.glsl"

layout(binding = 0) uniform sampler2D u_sampler_2D[1];

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(u_sampler_2D[0], vertex_in.tex_coord);
//    out_color.rgb = tonemap_exposure(out_color.rgb, 2.0);

}