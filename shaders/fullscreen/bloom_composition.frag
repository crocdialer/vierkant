#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../utils/tonemap.glsl"

#define COLOR 0
#define BLOOM 1

layout(binding = 0) uniform sampler2D u_sampler_2D[2];

layout(std140, binding = 1) uniform ubo_t
{
    float u_gamma;
    float u_exposure;
};

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 hdr_color = texture(u_sampler_2D[COLOR], vertex_in.tex_coord).rgb;
    vec3 bloom = texture(u_sampler_2D[BLOOM], vertex_in.tex_coord).rgb;

    // additive blending + tone mapping
    vec3 result = tonemap_exposure(hdr_color + bloom, u_exposure);

    // gamma correction
    result = pow(result, vec3(1.0 / u_gamma));
    out_color = vec4(result, 1.0);
}