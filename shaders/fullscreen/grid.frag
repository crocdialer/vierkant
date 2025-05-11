#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../utils/transform.glsl"

struct grid_params_t
{
    vec4 color;
};

layout(std140, binding = 0) uniform UBO { grid_params_t params; };

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
//    gl_FragDepth = ;
    // ray onto ground-plane, get uv
    out_color = vec4(vertex_in.tex_coord, 0, 1.0);//params.color * texture(u_sampler_2D[COLOR], vertex_in.tex_coord);
}