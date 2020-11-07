#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/importance_sampling.glsl"

layout(push_constant) uniform PushConstants
{
    render_context_t context;
};

layout(binding = 1) uniform samplerCube u_sampler_cube;

layout(location = 0) in VertexData
{
    vec3 eye_vec;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 N = normalize(vertex_in.eye_vec);

    // convolve the environment map with a cosine lobe along N
    out_color = vec4(ImportanceSampleDiffuse(N, u_sampler_cube), 1.0);
}