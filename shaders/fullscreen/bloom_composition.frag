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

vec4 sample_motion_blur(sampler2D tex, vec2 coord, vec2 motion)
{
    const uint max_num_taps = 8;
    float pixel_motion = length(motion * textureSize(tex, 0));
    uint num_taps = clamp(uint(pixel_motion + 0.5), 1, max_num_taps);

    vec4 color = texture(tex, coord);

    for(uint i = 1; i < num_taps; ++i)
    {
        vec2 offset = motion * (float(i) / (num_taps - 1) - 0.5);
        color += texture(tex, coord + offset);
    }
    return color / num_taps;
}

void main()
{
    float gain = u_motionblur_gain * clamp(u_time_delta / u_shutter_time, 0.0, 2.0);
    vec2 motion = gain * texture(u_sampler_2D[MOTION], vertex_in.tex_coord).rg;

    vec4 hdr_color = sample_motion_blur(u_sampler_2D[COLOR], vertex_in.tex_coord, motion);
    vec3 bloom = texture(u_sampler_2D[BLOOM], vertex_in.tex_coord).rgb;

    // additive blending + tone mapping
    vec3 result = tonemap_exposure(hdr_color.rgb + bloom, u_exposure);
//    vec3 result = tonemap_aces(hdr_color + bloom);

    // gamma correction
    result = pow(result, vec3(1.0 / u_gamma));
    out_color = vec4(result, hdr_color.a);
}