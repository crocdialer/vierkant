import "../renderer/types.slang";
import "../utils/transform.slang";

[[vk::push_constant]] renderer::push_constants_t push_constants;

[[vk::binding(renderer::ResourceBinding::RenderData, 0)]]
ConstantBuffer<renderer::render_data_t> render_data;

[[vk::binding(renderer::ResourceBinding::Textures, 0)]]
SamplerCube u_sampler_cube[1];

[[shader("vertex")]]
void vertex_main(float3 a_position: Position, uint instanceID: SV_StartInstanceLocation,
                 out float3 eye_vec, nointerpolation out uint material_index, out float4 sv_position: SV_Position)
{
    renderer::matrix_struct_t m = render_data.mesh_draws[instanceID].current_matrices;
    eye_vec = a_position.xyz;
    material_index = render_data.mesh_draws[instanceID].material_index;
    sv_position = mul(m.projection, float4(utils::apply_transform(m.transform, a_position), 1.0));
}

[[shader("pixel")]]
float4 fragment_main(in float3 eye_vec, nointerpolation in uint material_index) : SV_Target
{
    return u_sampler_cube[0].Sample(eye_vec) * render_data.materials[material_index].color;
}
