#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"
#include "../utils/dof.glsl"

#define COLOR 0
#define DEPTH 1

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(binding = 0) uniform sampler2D u_sampler_2D[2];

layout(std140, binding = 1) uniform dof_ubo
{
    dof_params_t dof_params;
};

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    gl_FragDepth = texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x;

    // depth of field
    out_color = depth_of_field(u_sampler_2D[COLOR], u_sampler_2D[DEPTH], vertex_in.tex_coord, context.size, dof_params);
}