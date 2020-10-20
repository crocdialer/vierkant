#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "utils/gaussian_blur.glsl"

#define COLOR 0

layout(binding = 0) uniform sampler2D u_sampler_2D[1];

layout(std140, binding = 1) uniform gaussian_ubo
{
    gaussian_ubo_t u_gaussian_weights;
};

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    // gaussian blur
    out_color = gaussian_blur(u_sampler_2D[COLOR], vertex_in.tex_coord, u_gaussian_weights);
}