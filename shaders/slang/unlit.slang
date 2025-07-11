import renderer;
import utils;

[[vk::push_constant]] render_context_t context;
[[vk::binding(ResourceBinding::MeshDraws, 0)]] StructuredBuffer<MeshDraw> draws;

[[shader("vertex")]]
void main(
    float3 a_position: Position,
    uint instanceID: SV_InstanceID,

    out IndexBundle index_bundle,
    out float4 sv_position: SV_Position
)
{
    index_bundle.mesh_draw_index = instanceID;
    index_bundle.material_index = draws[instanceID].material_index;

    MatrixStruct m = draws[index_bundle.mesh_draw_index].current_matrices;
    float3 world_pos = apply_transform(m.transform, a_position);
    sv_position = mul(m.projection, float4(world_pos, 1.0));
}