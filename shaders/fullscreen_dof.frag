#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "renderer/types.glsl"
#include "utils/dof.glsl"

#define COLOR 0
#define DEPTH 1

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(binding = 0) uniform sampler2D u_sampler_2D[2];

layout(std140, binding = 1) uniform dof_ubo
{
    dof_settings_t dof_settings;
};

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    gl_FragDepth = texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x;

//    float depth_linear = linearize(gl_FragDepth, context.clipping.x, context.clipping.y);
//    out_color = vec4(depth_linear, depth_linear, depth_linear, 1.0);

    // depth of field
    out_color = depth_of_field(u_sampler_2D[COLOR], u_sampler_2D[DEPTH], vertex_in.tex_coord, context.size,
                               context.clipping, dof_settings);
}