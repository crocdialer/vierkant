import "renderer/types.slang";
import "utils/transform.slang";

[[vk::push_constant]]
renderer::context_t context;
[[vk::binding(renderer::ResourceBinding::MeshDraws, 0)]]
StructuredBuffer<renderer::mesh_draw_t> draws;
[[vk::binding(renderer::ResourceBinding::Material, 0)]]
StructuredBuffer<renderer::material_t> materials;

[[shader("vertex")]]
void vertex_main(float3 a_position: Position, uint instanceID: SV_InstanceID,
                 nointerpolation out renderer::index_bundle_t index_bundle, out float4 sv_position: SV_Position)
{
    index_bundle.mesh_draw_index = instanceID;
    index_bundle.material_index = draws[instanceID].material_index;

    renderer::matrix_struct_t m = draws[index_bundle.mesh_draw_index].current_matrices;
    float3 world_pos = utils::apply_transform(m.transform, a_position);
    sv_position = mul(m.projection, float4(world_pos, 1.0));
}

[[shader("pixel")]]
float4 fragment_main(nointerpolation in renderer::index_bundle_t index_bundle, in float4 sv_position: SV_Position)
{
    return materials[index_bundle.material_index].color;
}
