#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in VertexData
{
    vec4 color;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vertex_in.color;
}