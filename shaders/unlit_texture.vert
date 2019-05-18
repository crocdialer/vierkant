#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 texture;
} ubo;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 0) out VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_out;

void main()
{
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(a_position, 1.0);
    vertex_out.color = a_color;
    vertex_out.tex_coord = (ubo.texture * vec4(a_tex_coord, 0, 1)).xy;
}