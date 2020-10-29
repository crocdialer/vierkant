#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "renderer/types.glsl"

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = BINDING_MATERIAL) uniform ubo_materials
{
    material_struct_t materials[MAX_NUM_DRAWABLES];
};

layout(binding = BINDING_TEXTURES) uniform samplerCube u_sampler_cube[1];

layout(location = 0) in VertexData
{
    vec3 eye_vec;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 dir = vertex_in.eye_vec;
    dir.y = -dir.y;
    vec3 hdr_color = texture(u_sampler_cube[0], dir).rgb;

    // tone mapping
    vec3 result = vec3(1.0) - exp(-hdr_color * context.exposure);

    // gamma correction
    result = pow(result, vec3(1.0 / context.gamma));
    out_color = vec4(result, 1.0);
}
