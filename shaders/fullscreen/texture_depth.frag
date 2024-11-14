#version 460
#extension GL_ARB_separate_shader_objects : enable

#define COLOR 0
#define DEPTH 1

layout(binding = 0) uniform sampler2D u_sampler_2D[2];

struct params_t
{
    vec4 color;
    float depth_scale;
    float depth_bias;
};

layout(std140, binding = 1) uniform UBO { params_t params; };

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    gl_FragDepth = params.depth_scale * texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x + params.depth_bias;
    out_color = params.color * texture(u_sampler_2D[COLOR], vertex_in.tex_coord);
}