import "../renderer/types.slang";
import "../utils/random.slang";
import "../utils/sampling.slang";
import "../utils/transform.slang";
import "../ray/visibility_test.slang";
import "../ray/ray_common.slang";

enum TextureIndex { DEPTH = 0, NORMAL = 1 };

[[vk::binding(0, 0)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 0)]] Sampler2D u_textures[2];

struct params_t
{
    float4x4 inverse_projection;
    utils::transform_t camera_transform;
    uint num_rays;
    float max_distance;
};
[[vk::binding(2, 0)]] StructuredBuffer<params_t> ubo;

[[vk::push_constant]]
renderer::context_t context;

float raytraced_occlusion(float3 position, float3 world_normal, float max_distance, uint num_rays, uint rng_state)
{
    const float tmin = 0.01, tmax = max_distance;

    float accumulated_ao = 0.0;
    float accumulated_factor = 0.0;

    float sample_offset = 2.0 * utils::rnd(rng_state) - 1.0;
    float3x3 frame = utils::local_frame(world_normal);

    for(uint i = 0; i < num_rays; ++i)
    {
        float2 Xi = frac(utils::Hammersley(i, num_rays) + float2(sample_offset));
        float3 cos_dir = utils::sample_hemisphere_cosine(Xi);
        float3 direction = mul(frame, cos_dir);

        ray::RayHit hit = ray::ray_cast(ray::Ray(position, tmin, direction, tmax), topLevelAS);
        float dist = hit.valid ? hit.hit_t : max_distance;

        float ao = min(dist, max_distance);
        float factor = 0.2 + 0.8 * cos_dir.z * cos_dir.z;
        accumulated_factor += factor;
        accumulated_ao += ao * factor;
    }
    accumulated_ao /= (max_distance * accumulated_factor);
    accumulated_ao *= accumulated_ao;
    return clamp(accumulated_ao, 0.0, 1.0);
}

[[shader("pixel")]]
float fragment_main(float2 tex_coord : TexCoord0, float4 sv_position : SV_Position) : SV_Target
{
    float depth = u_textures[TextureIndex::DEPTH].Sample(tex_coord).x;
    float3 normal = u_textures[TextureIndex::NORMAL].Sample(tex_coord).xyz;

    float3 clip_pos = float3(sv_position.xy / context.size, depth);
    float4 viewspace_pos = mul(ubo[0].inverse_projection, float4(2.0 * clip_pos.xy - 1.0, clip_pos.z, 1.0));
    float3 position = utils::apply_transform(ubo[0].camera_transform, viewspace_pos.xyz / viewspace_pos.w);

    return raytraced_occlusion(position, normal, ubo[0].max_distance, ubo[0].num_rays, context.random_seed);
}
