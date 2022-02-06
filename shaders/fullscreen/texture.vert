#version 460
#extension GL_ARB_separate_shader_objects : enable

// full-screen triangle
const vec2 positions[3] = vec2[](vec2(-1, -1), vec2(-1.0, 3.0), vec2(3.0, -1.0));
const vec2 tex_coords[3] = vec2[](vec2(0.0, 0.0), vec2(0.0, 2.0), vec2(2.0, 0.0));

layout(location = 0) out VertexData
{
    vec2 tex_coord;
} vertex_out;

void main()
{
    vertex_out.tex_coord = tex_coords[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}