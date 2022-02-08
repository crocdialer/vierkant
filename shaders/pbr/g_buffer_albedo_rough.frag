#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "../renderer/types.glsl"

#define ALBEDO 0
#define AO_ROUGH_METAL 1

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MATERIAL) readonly buffer MaterialBuffer
{
    material_struct_t materials[];
};

layout(set = 1, binding = BINDING_TEXTURES) uniform sampler2D u_sampler_2D[];

layout(location = 0) flat in uint object_index;
layout(location = 1) in VertexData
{
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
    vec4 current_position;
    vec4 last_position;
} vertex_in;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_emission;
layout(location = 3) out vec4 out_ao_rough_metal;
layout(location = 4) out vec2 out_motion;

void main()
{
    material_struct_t material = materials[object_index];

    out_color = vec4(1);
    out_emission = vec4(0);

    if(!context.disable_material)
    {
        vec4 tex_color = vertex_in.color * texture(u_sampler_2D[material.base_texture_index + ALBEDO],
                                                   vertex_in.tex_coord);
        float cut_off = (material.blend_mode == BLEND_MODE_MASK) ? material.alpha_cutoff : 0.f;
        if(tex_color.a < cut_off){ discard; }
        out_color = material.color * tex_color;
        out_emission = material.emission * tex_color;
    }

    out_normal = vec4(vertex_in.normal, 1.0);
    out_ao_rough_metal = vec4(texture(u_sampler_2D[material.base_texture_index + AO_ROUGH_METAL],
                                      vertex_in.tex_coord).xyz, 1.0);

    // motion
    out_motion = 0.5 * (vertex_in.current_position.xy / vertex_in.current_position.w - vertex_in.last_position.xy / vertex_in.last_position.w);
}