#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "renderer/types.glsl"

#define COLOR 0
#define BLOOM 1
#define DEPTH 2

layout(push_constant) uniform PushConstants
{
    render_context_t context;
};

layout(binding = 0) uniform sampler2D u_sampler_2D[3];

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 hdr_color = texture(u_sampler_2D[COLOR], vertex_in.tex_coord).rgb;
    vec3 bloom = texture(u_sampler_2D[BLOOM], vertex_in.tex_coord).rgb;

    // additive blending
    hdr_color += bloom;

    // tone mapping
    vec3 result = vec3(1.0) - exp(-hdr_color * context.exposure);

    // gamma correction
    result = pow(result, vec3(1.0 / context.gamma));
    out_color = vec4(result, 1.0);

    // passthru fragment depth
    gl_FragDepth = texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x;
}