#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

#define ALBEDO 0
#define NORMAL 1
#define AO_ROUGH_METAL 2
#define EMMISSION 3

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = BINDING_MATERIAL) uniform ubo_materials
{
    material_struct_t materials[MAX_NUM_DRAWABLES];
};

layout(binding = BINDING_TEXTURES) uniform sampler2D u_sampler_2D[4];

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
layout(location = 2) out vec4 out_emission;
layout(location = 3) out vec4 out_ao_rough_metal;

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
        out_emission = texture(u_sampler_2D[EMMISSION], vertex_in.tex_coord);
        out_emission.rgb *= 10.0 * out_emission.a;
    }

    vec3 normal = normalize(2.0 * (texture(u_sampler_2D[NORMAL], vertex_in.tex_coord.xy).xyz - vec3(0.5)));

    // normal, tangent, bi-tangent
    vec3 t = normalize(vertex_in.tangent);
    vec3 n = normalize(vertex_in.normal);
    vec3 b = normalize(cross(n, t));
    mat3 transpose_tbn = mat3(t, b, n);
    normal = transpose_tbn * normal;

    out_normal = vec4(normalize(normal), 1);
    out_ao_rough_metal = vec4(texture(u_sampler_2D[AO_ROUGH_METAL], vertex_in.tex_coord).xyz, 1.0);
}