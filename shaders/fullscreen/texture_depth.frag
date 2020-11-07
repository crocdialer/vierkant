#version 460
#extension GL_ARB_separate_shader_objects : enable

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
    out_color = texture(u_sampler_2D[COLOR], vertex_in.tex_coord);
}