#version 460
#extension GL_ARB_separate_shader_objects : enable

//! smooth threshold values
const float luma_thresh_min = 0.95;
const float luma_thresh_max = 1.1;

layout(binding = 0) uniform sampler2D u_sampler_2D[1];

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(u_sampler_2D[0], vertex_in.tex_coord);

    // NTSC luma formula
    const vec3 to_luma = vec3(0.299, 0.587, 0.114);
    float brightness = dot(out_color.rgb, to_luma);

    // apply smooth threshold
    out_color = mix(vec4(0.0, 0.0, 0.0, 1.0), out_color, smoothstep(luma_thresh_min, luma_thresh_max, brightness));
}