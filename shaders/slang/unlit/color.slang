import "../renderer/types.slang";
import "../utils/transform.slang";

[[vk::push_constant]]
renderer::push_constants_t push_constants;

[[vk::binding(renderer::ResourceBinding::RenderData, 0)]]
ConstantBuffer<renderer::render_data_t> render_data;

struct VertexData
{
    float4 color;
};

[[shader("vertex")]]
VertexData vertex_main(float3 a_position: Position, float4 a_color: Color, uint instanceID: SV_StartInstanceLocation,
                       nointerpolation out renderer::index_bundle_t indices, out float4 sv_position: SV_Position)
{
    VertexData vertex_out;
    indices.mesh_draw_index = instanceID;
    indices.material_index = render_data.mesh_draws[instanceID].material_index;

    renderer::matrix_struct_t m = render_data.mesh_draws[indices.mesh_draw_index].current_matrices;
    float3 world_pos = utils::apply_transform(m.transform, a_position);
    sv_position = mul(m.projection, float4(world_pos, 1.0));
    vertex_out.color = a_color;
    return vertex_out;
}

[[shader("pixel")]]
float4 fragment_main(nointerpolation in renderer::index_bundle_t indices, in VertexData vertex_in)
{
    uint material_index = indices.material_index;
    return render_data.materials[material_index].color * vertex_in.color;
}
