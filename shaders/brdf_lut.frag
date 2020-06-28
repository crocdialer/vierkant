#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "renderer/brdf.glsl"

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    float roughness = vertex_in.tex_coord.y;
    float NoV = vertex_in.tex_coord.x;
    out_color = vec4(IntegrateBRDF(roughness, NoV), 0.0, 1.0);
}