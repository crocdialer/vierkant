#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../utils/procedural_environment.glsl"

layout(location = 0) in VertexData
{
    vec3 eye_vec;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(environment_white(normalize(vertex_in.eye_vec)), 1.0);
}