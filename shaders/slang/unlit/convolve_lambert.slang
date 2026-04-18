import "../renderer/types.slang";
import "../renderer/importance_sampling.slang";
import "../utils/transform.slang";

[[vk::push_constant]] renderer::context_t context;

[[vk::binding(renderer::ResourceBinding::MeshDraws, 0)]]
StructuredBuffer<renderer::mesh_draw_t> draws;

[[vk::binding(1, 0)]] SamplerCube u_sampler_cube;

[[shader("vertex")]]
void vertex_main(float3 a_position: Position, uint instanceID: SV_StartInstanceLocation,
                 out float3 eye_vec, out float4 sv_position: SV_Position)
{
    renderer::matrix_struct_t m = draws[instanceID].current_matrices;
    eye_vec = a_position.xyz;
    sv_position = mul(m.projection, float4(utils::apply_transform(m.transform, a_position), 1.0));
}

[[shader("pixel")]]
float4 fragment_main(in float3 eye_vec) : SV_Target
{
    float3 N = normalize(eye_vec);
    return float4(renderer::ImportanceSampleDiffuse(N, u_sampler_cube), 1.0);
}
