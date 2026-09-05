import "../renderer/types.slang";
import "../utils/transform.slang";

[[vk::push_constant]]
renderer::push_constants_t push_constants;

[[vk::binding(renderer::ResourceBinding::RenderData, 0)]]
ConstantBuffer<renderer::render_data_t> render_data;

[[shader("vertex")]]
void vertex_main(float3 a_position: Position, uint instanceID: SV_StartInstanceLocation,
                 nointerpolation out renderer::index_bundle_t indices, out float4 sv_position: SV_Position)
{
    indices.mesh_draw_index = instanceID;
    indices.material_index = render_data.mesh_draws[instanceID].material_index;

    renderer::matrix_struct_t m = render_data.mesh_draws[indices.mesh_draw_index].current_matrices;
    float3 world_pos = utils::apply_transform(m.transform, a_position);
    sv_position = mul(m.projection, float4(world_pos, 1.0));
}

[[shader("pixel")]]
float4 fragment_main(nointerpolation in renderer::index_bundle_t indices, in float4 sv_position: SV_Position)
{
    return render_data.materials[indices.material_index].color;
}
