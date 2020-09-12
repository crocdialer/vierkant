#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "utils/fxaa.glsl"

#define COLOR 0

layout(binding = 0) uniform sampler2D u_sampler_2D[1];

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    // fxaa
    fxaa_settings_t settings = fxaa_default_settings;
    out_color = fxaa(u_sampler_2D[COLOR], vertex_in.tex_coord, settings);
}