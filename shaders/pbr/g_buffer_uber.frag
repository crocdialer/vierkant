#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "../renderer/types.glsl"

#define ALBEDO 0
#define AO_ROUGH_METAL 1
#define NORMAL 2
#define EMMISSION 3

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
        vec4 tex_color = vertex_in.color;

        if((material.texture_type_flags & TEXTURE_TYPE_COLOR) != 0)
        {
            tex_color *= texture(u_sampler_2D[material.base_texture_index + ALBEDO], vertex_in.tex_coord);
        }

        float cut_off = (material.blend_mode == BLEND_MODE_MASK) ? material.alpha_cutoff : 0.f;
        if(tex_color.a < cut_off){ discard; }
        out_color = material.color * tex_color;

        out_emission = material.emission;

        if((material.texture_type_flags & TEXTURE_TYPE_EMISSION) != 0)
        {
            out_emission = texture(u_sampler_2D[material.base_texture_index + EMMISSION], vertex_in.tex_coord);
            out_emission.a *= material.emission.a;
        }
    }

    vec3 normal = vertex_in.normal;

    if((material.texture_type_flags & TEXTURE_TYPE_NORMAL) != 0)
    {
        normal = normalize(2.0 * (texture(u_sampler_2D[material.base_texture_index + NORMAL],
        vertex_in.tex_coord.xy).xyz - vec3(0.5)));

        // normal, tangent, bi-tangent
        vec3 t = normalize(vertex_in.tangent);
        vec3 n = normalize(vertex_in.normal);
        vec3 b = normalize(cross(n, t));
        mat3 transpose_tbn = mat3(t, b, n);
        normal = transpose_tbn * normal;
    }
    out_normal = vec4(normalize(normal), 1);

    out_ao_rough_metal = vec4(material.ambient, material.roughness, material.metalness, 1);

    if((material.texture_type_flags & TEXTURE_TYPE_AO_ROUGH_METAL) != 0)
    {
        out_ao_rough_metal = vec4(texture(u_sampler_2D[material.base_texture_index + AO_ROUGH_METAL],
                                          vertex_in.tex_coord).xyz, 1.0);
    }

    // motion
    out_motion = 0.5 * (vertex_in.current_position.xy / vertex_in.current_position.w - vertex_in.last_position.xy / vertex_in.last_position.w);
}