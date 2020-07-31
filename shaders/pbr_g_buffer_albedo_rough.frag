#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "renderer/types.glsl"

#define ALBEDO 0
#define AO_ROUGH_METAL 1

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = BINDING_MATERIAL) uniform ubo_materials
{
    material_struct_t materials[MAX_NUM_DRAWABLES];
};

layout(binding = BINDING_TEXTURES) uniform sampler2D u_sampler_2D[3];

layout(location = 0) in VertexData
{
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec3 eye_vec;
} vertex_in;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_position;
layout(location = 3) out vec4 out_emission;
layout(location = 4) out vec4 out_ao_rough_metal;

void main()
{
    material_struct_t material = materials[context.material_index];

    out_color = vec4(1);
    out_emission = vec4(0);

    if(context.disable_material == 0)
    {
        vec4 tex_color = vertex_in.color * texture(u_sampler_2D[ALBEDO], vertex_in.tex_coord);
        if(smoothstep(0.0, 1.0, tex_color.a) < 0.01){ discard; }
        out_color = material.color * tex_color;
        out_emission = material.emission * tex_color;
    }

    out_normal = vec4(vertex_in.normal, 1.0);
    out_position = vec4(vertex_in.eye_vec, 1);
    out_ao_rough_metal = vec4(texture(u_sampler_2D[AO_ROUGH_METAL], vertex_in.tex_coord).xyz, 1.0);
}