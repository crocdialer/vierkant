import "../renderer/types.slang";
import "../renderer/point_light.slang";
import "../utils/bsdf.slang";
import "../utils/constants.slang";

[[vk::push_constant]] ConstantBuffer<renderer::context_t> context;

struct ubo_t
{
    float4x4 camera_transform;
    float4x4 inverse_projection;
    uint num_mip_levels;
    float environment_factor;
    uint num_lights;
};
[[vk::binding(0, 0)]] ConstantBuffer<ubo_t> ubo;

enum GBufferIndex { ALBEDO = 0, NORMAL = 1, EMISSION = 2, AO_ROUGH_METAL = 3, OCCLUSION = 4, MOTION = 5, DEPTH = 6, BRDF_LUT = 7 };
[[vk::binding(1, 0)]] Sampler2D u_sampler_2D[8];

enum EnvIndex { ENV_DIFFUSE = 0, ENV_SPEC = 1 };
[[vk::binding(2, 0)]] SamplerCube u_sampler_cube[2];

[[vk::binding(3, 0)]] StructuredBuffer<renderer::lightsource_t> lights;

float3 compute_enviroment_lighting(float3 V, float3 N, float3 albedo, float roughness, float metalness,
                                   float ambient_occlusion)
{
    float3 irradiance = u_sampler_cube[EnvIndex::ENV_DIFFUSE].Sample(N).rgb * utils::ONE_OVER_PI;

    float3 R = normalize(reflect(V, N));
    float spec_mip_lvl = roughness * float(ubo.num_mip_levels - 1);
    float3 reflection = u_sampler_cube[EnvIndex::ENV_SPEC].SampleLevel(R, spec_mip_lvl).rgb;

    float NoV = clamp(dot(N, V), 0.0, 1.0);
    float2 brdf = u_sampler_2D[GBufferIndex::BRDF_LUT].Sample(float2(NoV, roughness)).rg;

    const float3 dielectricF0 = float3(0.04);
    float3 F0 = lerp(dielectricF0, albedo, metalness);
    float3 F = utils::F_SchlickR(max(NoV, 0.0), F0, roughness);

    float3 specular = reflection * (F * brdf.x + brdf.y);

    float3 diffuse = irradiance * albedo;
    diffuse *= 1.0 - metalness;

    float3 ambient = diffuse + F0 * specular * ambient_occlusion;

    return ambient;
}

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0, float4 sv_position : SV_Position) : SV_Target
{
    float depth = u_sampler_2D[GBufferIndex::DEPTH].Sample(tex_coord).x;

    float3 clip_pos = float3(sv_position.xy / context.size, depth);
    float4 viewspace_pos = mul(ubo.inverse_projection, float4(2.0 * clip_pos.xy - 1, clip_pos.z, 1));
    float3 position = mul(ubo.camera_transform, viewspace_pos / viewspace_pos.w).xyz;

    float3 V = normalize(position - mul(ubo.camera_transform, float4(0, 0, 0, 1)).xyz);

    float4 albedo = u_sampler_2D[GBufferIndex::ALBEDO].Sample(tex_coord);
    float3 normal = normalize(u_sampler_2D[GBufferIndex::NORMAL].Sample(tex_coord).xyz);
    float3 ao_rough_metal = u_sampler_2D[GBufferIndex::AO_ROUGH_METAL].Sample(tex_coord).rgb;
    float3 emission = u_sampler_2D[GBufferIndex::EMISSION].Sample(tex_coord).rgb;
    float occlusion = u_sampler_2D[GBufferIndex::OCCLUSION].Sample(tex_coord).x;

    float3 env_color = compute_enviroment_lighting(V, normal, albedo.rgb, ao_rough_metal.g, ao_rough_metal.b, ao_rough_metal.r);
    env_color *= occlusion;

    float3 punctial_light_color = float3(0);

    for(uint i = 0; i < ubo.num_lights; ++i)
    {
        punctial_light_color += renderer::shade(lights[i], V, normal, position, albedo,
                                                ao_rough_metal.g, ao_rough_metal.b, 1.0);
    }
    return float4(punctial_light_color + env_color * ubo.environment_factor + emission, 1.0);
}
