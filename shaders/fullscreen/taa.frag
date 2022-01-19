#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../utils/taa.glsl"

#define COLOR 0
#define DEPTH 1
#define MOTION 2
#define HISTORY_COLOR 3
#define HISTORY_DEPTH 4

struct taa_ubo_t
{
    float near;
    float far;
};

layout(binding = 0) uniform sampler2D u_sampler_2D[5];

layout(std140, binding = 1) uniform taa_ubo
{
    taa_ubo_t u_settings;
};

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 taa_color = taa(vertex_in.tex_coord,
                         u_sampler_2D[COLOR],
                         u_sampler_2D[DEPTH],
                         u_sampler_2D[MOTION],
                         u_sampler_2D[HISTORY_COLOR],
                         u_sampler_2D[HISTORY_DEPTH],
                         u_settings.near,
                         u_settings.far);

    out_color = vec4(taa_color, 1.0);
}