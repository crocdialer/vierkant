import "../renderer/types.slang";
import "../utils/random.slang";
import "../utils/constants.slang";
import "../utils/color.slang";

[[vk::push_constant]]
renderer::context_t context;

[[vk::binding(renderer::ResourceBinding::Material, 0)]]
StructuredBuffer<renderer::material_t> materials;

[[vk::binding(renderer::ResourceBinding::Textures, 1)]]
Sampler2D textures[];

struct FragOutput
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 emission : SV_Target2;
    float4 ao_rough_metal : SV_Target3;
    float2 motion : SV_Target4;
    uint object_id : SV_Target5;
};

uint tex_offset(uint type, uint flags) { return countbits(flags & (type - 1)); }

[[shader("pixel")]]
FragOutput fragment_main(nointerpolation in renderer::index_bundle_t indices,
                         in renderer::g_buffer_vertex_data_t vertex_in, in float4 sv_position: SV_Position,
                         in bool is_front_face: SV_IsFrontFace, in float3 bary_coord: SV_Barycentrics)
{
    FragOutput result;

    result.object_id = utils::MAX_UINT16 - indices.mesh_draw_index;

    uint rng_state = utils::tea(context.random_seed, uint(context.size.x * sv_position.y + sv_position.x));

    renderer::material_t material = materials[indices.material_index];

    float3 normal = (material.two_sided && !is_front_face) ? -vertex_in.normal : vertex_in.normal;
    result.normal = float4(normal, 1);

    result.color = float4(1);
    result.emission = float4(0);
    result.ao_rough_metal = float4(material.ambient, material.roughness, material.metalness, 1);

    result.motion = 0.5 * (vertex_in.current_position.xy / vertex_in.current_position.w -
                           vertex_in.last_position.xy / vertex_in.last_position.w);

    if(context.debug_flags != 0)
    {
        if(renderer::has_flag(context.debug_flags, (uint) renderer::DebugFlagBits::DRAW_ID))
        {
            uint obj_hash = utils::tea(indices.mesh_draw_index, indices.meshlet_index);
            result.color.rgb =
                    float3(float(obj_hash & 255), float((obj_hash >> 8) & 255), float((obj_hash >> 16) & 255)) / 255.0;
        }

        if(renderer::has_flag(context.debug_flags, (uint) renderer::DebugFlagBits::LOD))
        {
            float lod_factor = 1.0 - (indices.lod_index / float(indices.lod_count));
            result.color.rgb = utils::jet(lod_factor);
        }

        result.ao_rough_metal.b = 0.0;

        float min_bary = min(min(bary_coord.x, bary_coord.y), bary_coord.z);
        float edge = smoothstep(0.045, 0.055, min_bary);
        edge = lerp(edge, 1.0, smoothstep(0.15, 0.25, fwidth(dot(bary_coord.xy, bary_coord.xy))));
        result.color.rgb *= edge;
        return result;
    }

    if(!context.disable_material)
    {
        result.color = material.color;

        if((material.texture_type_flags & (uint) renderer::TextureType::Color) != 0)
        {
            uint offset = tex_offset((uint) renderer::TextureType::Color, material.texture_type_flags);
            result.color *=
                    textures[NonUniformResourceIndex(material.base_texture_index + offset)].Sample(vertex_in.tex_coord);
        }

        if(material.blend_mode == renderer::BlendMode::Mask && result.color.a < material.alpha_cutoff) { discard; }

        if(material.blend_mode == renderer::BlendMode::Blend)
        {
            result.color.a *= 1.0 - material.transmission;
            if(utils::rnd(rng_state) >= result.color.a) { discard; }
        }
        result.color.a = 1.0;

        result.emission = material.emission;

        if((material.texture_type_flags & (uint) renderer::TextureType::Emission) != 0)
        {
            uint offset = tex_offset((uint) renderer::TextureType::Emission, material.texture_type_flags);
            result.emission.rgb = textures[NonUniformResourceIndex(material.base_texture_index + offset)]
                                          .Sample(vertex_in.tex_coord)
                                          .rgb;
        }
        result.emission.rgb *= result.emission.a;
    }

    if((material.texture_type_flags & (uint) renderer::TextureType::Normal) != 0)
    {
        uint offset = tex_offset((uint) renderer::TextureType::Normal, material.texture_type_flags);
        normal.xy = 2.0 * (textures[NonUniformResourceIndex(material.base_texture_index + offset)]
                                   .Sample(vertex_in.tex_coord)
                                   .xy -
                           float2(0.5));
        normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);

        float3 t = normalize(vertex_in.tangent);
        float3 n = normalize(vertex_in.normal);
        float3 b = normalize(cross(n, t));
        // mul(v, M) with row-major M(t,b,n) is equivalent to GLSL mat3(t,b,n) * v (column-major)
        normal = mul(normal, float3x3(t, b, n));
        result.normal = float4(normalize(normal), 1);
    }

    if((material.texture_type_flags & (uint) renderer::TextureType::AoRoughMetal) != 0)
    {
        uint offset = tex_offset((uint) renderer::TextureType::AoRoughMetal, material.texture_type_flags);
        result.ao_rough_metal = float4(
                textures[NonUniformResourceIndex(material.base_texture_index + offset)].Sample(vertex_in.tex_coord).xyz,
                1.0);
    }

    return result;
}
