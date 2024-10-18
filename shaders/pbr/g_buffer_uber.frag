#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_fragment_shader_barycentric : require

#include "g_buffer_vertex_data.glsl"
#include "../renderer/types.glsl"

// rnd(state)
#include "../utils/random.glsl"
#include "../utils/constants.glsl"

#define ALBEDO 0
#define NORMAL 1
#define AO_ROUGH_METAL 2
#define EMMISSION 3

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, set = 0, binding = BINDING_MATERIAL) readonly buffer MaterialBuffer
{
    material_struct_t materials[];
};

layout(set = 1, binding = BINDING_TEXTURES) uniform sampler2D u_sampler_2D[];

layout(location = LOCATION_INDEX_BUNDLE) flat in index_bundle_t indices;
layout(location = LOCATION_VERTEX_BUNDLE) in VertexData
{
    g_buffer_vertex_data_t vertex_in;
};

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_emission;
layout(location = 3) out vec4 out_ao_rough_metal;
layout(location = 4) out vec2 out_motion;
layout(location = 5) out uint out_object_id;

//! return a texture's index by counting all set flagbits
uint tex_offset(uint type, uint flags)
{
    uint ret = 0;
    uint msb = findMSB(type);

    for(uint i = 0; i < msb; ++i)
    {
        if((flags & (TEXTURE_TYPE_COLOR << i)) > 0){ ret++; }
    }
    return ret;
}

void main()
{
    out_object_id = MAX_UINT16 - indices.mesh_draw_index;

    uint rng_state = tea(context.random_seed, uint(context.size.x * gl_FragCoord.y + gl_FragCoord.x));

    material_struct_t material = materials[indices.material_index];

    // invert normals for two-sided/backface surfels
    vec3 normal = (material.two_sided && !gl_FrontFacing) ? -vertex_in.normal : vertex_in.normal;
    out_normal = vec4(normal, 1);

    out_color = vec4(1);
    out_emission = vec4(0);
    out_ao_rough_metal = vec4(material.ambient, material.roughness, material.metalness, 1);

    // motion
    out_motion = 0.5 * (vertex_in.current_position.xy / vertex_in.current_position.w - vertex_in.last_position.xy / vertex_in.last_position.w);

    // debug object-ids
    if(context.debug_draw_ids)
    {
        uint obj_hash = tea(indices.mesh_draw_index, indices.meshlet_index);// gl_PrimitiveID
        out_color.rgb = vec3(float(obj_hash & 255), float((obj_hash >> 8) & 255), float((obj_hash >> 16) & 255)) / 255.0;

        // no metallic
        out_ao_rough_metal.b = 0.0;

        // black triangle edges, fade out for small/micro-triangles
        float min_bary = min(min(gl_BaryCoordEXT.x, gl_BaryCoordEXT.y), gl_BaryCoordEXT.z);
        float edge = smoothstep(0.045, 0.055, min_bary);
        edge = mix(edge, 1.0, smoothstep(0.15, 0.25, fwidth(dot(gl_BaryCoordEXT.xy, gl_BaryCoordEXT.xy))));
        out_color.rgb *= edge;
        return;
    }

    if(!context.disable_material)
    {
        out_color = material.color;
        out_color.a *= 1.0 - material.transmission;

        if((material.texture_type_flags & TEXTURE_TYPE_COLOR) != 0)
        {
            uint offset = tex_offset(TEXTURE_TYPE_COLOR, material.texture_type_flags);
            out_color *= texture(u_sampler_2D[material.base_texture_index + offset], vertex_in.tex_coord);
        }

        // apply alpha-cutoff
        if(material.blend_mode == BLEND_MODE_MASK && out_color.a < material.alpha_cutoff){ discard; }

        // apply stochastic dithering
        if(material.blend_mode == BLEND_MODE_BLEND && rnd(rng_state) >= out_color.a){ discard; }
        out_color.a = 1.0;

        out_emission = material.emission;

        if((material.texture_type_flags & TEXTURE_TYPE_EMISSION) != 0)
        {
            uint offset = tex_offset(TEXTURE_TYPE_EMISSION, material.texture_type_flags);
            out_emission.rgb = texture(u_sampler_2D[material.base_texture_index + offset], vertex_in.tex_coord).rgb;
        }
        out_emission.rgb *= out_emission.a;
    }

    if((material.texture_type_flags & TEXTURE_TYPE_NORMAL) != 0)
    {
        uint offset = tex_offset(TEXTURE_TYPE_NORMAL, material.texture_type_flags);
        normal = normalize(2.0 * (texture(u_sampler_2D[material.base_texture_index + offset],
        vertex_in.tex_coord.xy).xyz - vec3(0.5)));

        // normal, tangent, bi-tangent
        vec3 t = normalize(vertex_in.tangent);
        vec3 n = normalize(vertex_in.normal);
        vec3 b = normalize(cross(n, t));
        mat3 transpose_tbn = mat3(t, b, n);
        normal = transpose_tbn * normal;
        out_normal = vec4(normalize(normal), 1);
    }

    if((material.texture_type_flags & TEXTURE_TYPE_AO_ROUGH_METAL) != 0)
    {
        uint offset = tex_offset(TEXTURE_TYPE_AO_ROUGH_METAL, material.texture_type_flags);
        out_ao_rough_metal = vec4(texture(u_sampler_2D[material.base_texture_index + offset],
                                          vertex_in.tex_coord).xyz, 1.0);
    }
}