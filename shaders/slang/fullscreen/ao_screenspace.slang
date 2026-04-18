import "../renderer/types.slang";
import "../utils/hash.slang";
import "../utils/random.slang";
import "../utils/sampling.slang";
import "../utils/transform.slang";

[[vk::constant_id(0)]] const int SSAO_KERNEL_SIZE = 9;

enum TextureIndex { DEPTH = 0, NORMAL = 1 };

struct params_t
{
    float4x4 projection;
    float4x4 inverse_projection;
    utils::transform_t view_transform;
    float ssao_radius;
    uint random_seed;
};
[[vk::binding(0, 0)]] StructuredBuffer<params_t> ubo;
[[vk::binding(1, 0)]] Sampler2D u_textures[2];

[[vk::push_constant]]
renderer::context_t context;

float screenspace_occlusion(float2 coord, float3 eye_normal, float ssao_radius, uint rng_state)
{
    float depth = u_textures[TextureIndex::DEPTH].Sample(coord).x;

    float3 clip_pos = float3(coord, depth);
    float4 viewspace_pos = mul(ubo[0].inverse_projection, float4(2.0 * clip_pos.xy - 1.0, clip_pos.z, 1.0));
    float3 position = viewspace_pos.xyz / viewspace_pos.w;

    float occlusion = 0.0;
    const float bias = 0.00025;

    float sample_offset = 2.0 * utils::rnd(rng_state) - 1.0;
    float3x3 frame = utils::local_frame(eye_normal);

    for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        float2 Xi = frac(utils::Hammersley(i, SSAO_KERNEL_SIZE) + float2(sample_offset));
        float3 direction = mul(frame, utils::sample_hemisphere_cosine(Xi));

        float3 new_pos = position + direction * ssao_radius;
        float4 clip_coord = mul(ubo[0].projection, float4(new_pos, 1.0));
        clip_coord /= clip_coord.w;
        float2 sample_coord = clip_coord.xy * 0.5 + 0.5;

        float sample_depth = u_textures[TextureIndex::DEPTH].Sample(sample_coord).x;
        float range_check = smoothstep(0.0, 1.0, ssao_radius / abs(position.z - new_pos.z));

        occlusion += (sample_depth >= (depth + bias) ? 1.0 : 0.0) * range_check;
    }
    return 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
}

[[shader("pixel")]]
float fragment_main(float2 tex_coord : TexCoord0, float4 sv_position : SV_Position) : SV_Target
{
    uint rng_state = utils::xxhash32(ubo[0].random_seed,
                                     uint(context.size.x * sv_position.y + sv_position.x));

    float3 eye_normal = u_textures[TextureIndex::NORMAL].Sample(tex_coord).xyz;
    eye_normal = utils::apply_rotation(ubo[0].view_transform, eye_normal);

    return screenspace_occlusion(tex_coord, eye_normal, ubo[0].ssao_radius, rng_state);
}
