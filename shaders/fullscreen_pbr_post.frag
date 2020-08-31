#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "utils/fxaa.glsl"
#include "utils/dof.glsl"

#define COLOR 0
#define DEPTH 1

layout(binding = 0) uniform sampler2D u_sampler_2D[2];

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    gl_FragDepth = texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x;

    // fxaa
    fxaa_settings_t settings = fxaa_default_settings;
    out_color = fxaa(u_sampler_2D[COLOR], vertex_in.tex_coord, settings);
}