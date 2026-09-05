import "../renderer/types.slang";
import "../utils/transform.slang";
import "../utils/constants.slang";

[[vk::binding(renderer::ResourceBinding::RenderData, 0)]]
ConstantBuffer<renderer::render_data_t> render_data;

[[vk::binding(1, 0)]] Sampler2D u_sampler_2D[1];

float2 panorama(float3 ray)
{
    return float2(0.5 + 0.5 * atan2(ray.x, -ray.z) * utils::ONE_OVER_PI, acos(ray.y) * utils::ONE_OVER_PI);
}

[[shader("vertex")]]
void vertex_main(float3 a_position: Position, uint instanceID: SV_StartInstanceLocation,
                 out float3 eye_vec, out float4 sv_position: SV_Position)
{
    renderer::matrix_struct_t m = render_data.mesh_draws[instanceID].current_matrices;
    eye_vec = a_position.xyz;
    sv_position = mul(m.projection, float4(utils::apply_transform(m.transform, a_position), 1.0));
}

[[shader("pixel")]]
float4 fragment_main(in float3 eye_vec) : SV_Target
{
    return u_sampler_2D[0].Sample(panorama(normalize(eye_vec)));
}
