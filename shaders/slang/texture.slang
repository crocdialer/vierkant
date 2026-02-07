import "renderer/types.slang";
import "utils/transform.slang";

[[vk::push_constant]]
render_context_t context;

[[vk::binding(ResourceBinding::MeshDraws, 0)]]
StructuredBuffer<mesh_draw_t> draws;

[[vk::binding(ResourceBinding::Material, 0)]]
StructuredBuffer<material_t> materials;

[[vk::binding(ResourceBinding::Textures, 0)]]
Sampler2D textures[];

struct VertexData
{
    float4 color;
    float2 tex_coord;
};

[[shader("vertex")]]
VertexData vertex_main(float3 a_position: Position, float4 a_color: Color, float2 uv: UV, uint instanceID: SV_InstanceID,
                 nointerpolation out index_bundle_t indices, out float4 sv_position: SV_Position)
{
    VertexData vertex_out;
    indices.mesh_draw_index = instanceID;
    indices.material_index = draws[instanceID].material_index;

    matrix_struct_t m = draws[indices.mesh_draw_index].current_matrices;
    float3 world_pos = apply_transform(m.transform, a_position);
    sv_position = mul(m.projection, float4(world_pos, 1.0));
    vertex_out.color = a_color;
    vertex_out.tex_coord = uv;
    return vertex_out;
}

[[shader("pixel")]]
float4 fragment_main(nointerpolation in index_bundle_t indices, in VertexData vertex_in)
{
    uint material_index = indices.mesh_draw_index;
    float4 tex_color = textures[materials[material_index].base_texture_index].Sample(vertex_in.tex_coord);
    return tex_color * materials[material_index].color * vertex_in.color;
}
