#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

#define ALBEDO 0

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = BINDING_MATERIAL) uniform ubo_materials
{
    material_struct_t materials[MAX_NUM_DRAWABLES];
};

layout(binding = BINDING_TEXTURES) uniform sampler2D u_sampler_2D[1];

layout(location = 0) in VertexData
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
    material_struct_t material = materials[context.material_index];

    out_color = vec4(1);
    out_emission = vec4(0);

    if(!context.disable_material)
    {
        vec4 tex_color = vertex_in.color * texture(u_sampler_2D[ALBEDO], vertex_in.tex_coord);
        float cut_off = (material.blend_mode == BLEND_MODE_MASK) ? material.alpha_cutoff : 0.f;
        if(tex_color.a < cut_off){ discard; }
        out_color = material.color * tex_color;
        out_emission = material.emission * tex_color;
    }

    out_normal = vec4(normalize(vertex_in.normal), 1);
    out_ao_rough_metal = vec4(material.ambient, material.roughness, material.metalness, 1);

    // motion
    out_motion = vertex_in.current_position.xy / vertex_in.current_position.w -
                 vertex_in.last_position.xy / vertex_in.last_position.w;
}