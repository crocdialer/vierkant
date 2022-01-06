#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../utils/tonemap.glsl"

#define COLOR 0
#define BLOOM 1
#define MOTION 2

layout(binding = 0) uniform sampler2D u_sampler_2D[3];

layout(std140, binding = 1) uniform ubo_t
{
    // tonemapping
    float u_gamma;
    float u_exposure;

    // motionblur
    float u_time_delta;
    float u_shutter_time;
    float u_motionblur_gain;
};

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

vec3 sample_motion_blur(sampler2D tex, vec2 coord, vec2 motion)
{
    const uint num_taps = 8;

    vec2 motion_inc = -motion / num_taps;
    vec3 color = vec3(0);

    for(uint i = 0; i < num_taps; ++i)
    {
        color += texture(tex, coord + i * motion_inc).rgb;
    }
    return color / num_taps;
}

void main()
{
    vec2 motion = u_motionblur_gain * (u_shutter_time / u_time_delta) * texture(u_sampler_2D[MOTION], vertex_in.tex_coord).rg;

    vec3 hdr_color = sample_motion_blur(u_sampler_2D[COLOR], vertex_in.tex_coord, motion);//texture(u_sampler_2D[COLOR], vertex_in.tex_coord).rgb;

    vec3 bloom = texture(u_sampler_2D[BLOOM], vertex_in.tex_coord).rgb;

    // additive blending + tone mapping
    vec3 result = tonemap_exposure(hdr_color + bloom, u_exposure);

    // gamma correction
    result = pow(result, vec3(1.0 / u_gamma));
    out_color = vec4(result, 1.0);
}