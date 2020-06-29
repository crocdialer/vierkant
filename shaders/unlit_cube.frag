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
    out_color = texture(u_sampler_cube[0], vertex_in.eye_vec);
    if(context.gamma != 1.0){ out_color.rgb = pow(out_color.rgb, vec3(1.0 / context.gamma)); }
}