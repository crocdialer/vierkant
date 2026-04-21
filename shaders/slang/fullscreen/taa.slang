import "../utils/color.slang";
import "../utils/camera.slang";

enum TextureIndex { COLOR = 0, DEPTH = 1, MOTION = 2, HISTORY_COLOR = 3, HISTORY_DEPTH = 4 };

[[vk::binding(0, 0)]] Sampler2D u_textures[5];

struct taa_ubo_t
{
    utils::camera_t camera;
    utils::camera_t last_camera;
};
[[vk::binding(1, 0)]] ConstantBuffer<taa_ubo_t> u_taa;

struct aabb_t { float3 min, max; };

float linear_depth(float depth, float near, float far)
{
    return (near / depth - near) / (far - near);
}

float3 luma_weight(float3 color)
{
    float luma = dot(utils::LuminanceNTSC, color);
    return color / (1.0 + luma);
}

float3 luma_weight_inverse(float3 color)
{
    float luma = dot(utils::LuminanceNTSC, color);
    return color / (1.0 - luma);
}

float3 clip_aabb(float3 aabb_min, float3 aabb_max, float3 c)
{
    float3 p_clip = 0.5 * (aabb_max + aabb_min);
    float3 e_clip = 0.5 * (aabb_max - aabb_min);
    float3 v_clip = c - p_clip;
    float3 v_unit = v_clip.xyz / e_clip;
    float3 a_unit = abs(v_unit);
    float max_unit = max(a_unit.x, max(a_unit.y, a_unit.z));
    return max_unit > 1.0 ? p_clip + v_clip / max_unit : c;
}

aabb_t neighbourhood_variance(float2 coord)
{
    uint tw, th;
    u_textures[TextureIndex::COLOR].GetDimensions(tw, th);
    const float2 texel = 1.0 / float2(tw, th);

    aabb_t ret;

    float3 neighbourhood[9];
    uint n = 0;
    float3 sum = float3(0);

    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            float3 c = mul(utils::RGB2YCoCg, luma_weight(u_textures[TextureIndex::COLOR].Sample(coord + texel * float2(x, y)).rgb));
            sum += c;
            neighbourhood[n++] = c;
        }
    }
    float3 mean = sum / 9.0;
    float3 sq_sum = float3(0);

    for(uint i = 0; i < 9; ++i)
    {
        float3 diff = neighbourhood[i] - mean;
        sq_sum += diff * diff;
    }
    float3 stddev = sqrt(sq_sum / 9.0);

    float variance_factor = 1.25;
    stddev *= variance_factor;

    ret.min = mean - stddev;
    ret.max = mean + stddev;
    return ret;
}

float4 taa(float2 in_coord, utils::camera_t taa_settings)
{
    float2 coord = in_coord - taa_settings.sample_offset;
    uint tw, th;
    u_textures[TextureIndex::COLOR].GetDimensions(tw, th);
    float2 texSize = float2(tw, th);
    float2 texel = 1.0 / texSize;

    float4 color = u_textures[TextureIndex::COLOR].Sample(coord);
    color.rgb = luma_weight(color.rgb);

    float min_depth = u_textures[TextureIndex::DEPTH].Load(int3(int2(coord * texSize), 0)).x;
    float2 min_depth_coord = coord;

    float2 diagonal_cross[4];
    diagonal_cross[0] = float2(-1, -1);
    diagonal_cross[1] = float2(1, 1);
    diagonal_cross[2] = float2(-1, 1);
    diagonal_cross[3] = float2(1, -1);

    for(uint o = 0; o < 4; ++o)
    {
        float2 d_coord = coord + texel * diagonal_cross[o];
        float d = u_textures[TextureIndex::DEPTH].Load(int3(int2(d_coord * texSize), 0)).x;

        if(d > min_depth)
        {
            min_depth = d;
            min_depth_coord = d_coord;
        }
    }

    const float max_motion_delta = 0.15;
    float2 motion_delta = min(u_textures[TextureIndex::MOTION].Sample(min_depth_coord).xy, max_motion_delta);

    float2 history_coord = in_coord - motion_delta;
    float3 history_color = luma_weight(u_textures[TextureIndex::HISTORY_COLOR].Sample(history_coord).rgb);

    float history_depth = 1.0;
    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            float2 d_coord = history_coord + texel * float2(x, y);
            float d = u_textures[TextureIndex::HISTORY_DEPTH].Load(int3(int2(d_coord * texSize), 0)).x;
            history_depth = min(history_depth, d);
        }
    }

    float alpha = 0.1;

    alpha += min(0.02 * length(motion_delta / texel), 0.25);

    if(any(history_coord < float2(0)) || any(history_coord > float2(1))) { alpha = 1.0; }

    float depth_delta = abs(linear_depth(min_depth, taa_settings.near, taa_settings.far) -
        linear_depth(history_depth, taa_settings.near, taa_settings.far));

    const float depth_eps = 5.0e-3;
    float depth_reject = depth_delta > depth_eps ? .1 : 0.0;
    alpha += depth_reject;

    aabb_t variance_aabb = neighbourhood_variance(coord);
    float3 history_ycocg = mul(utils::RGB2YCoCg, history_color);
    float3 rectified_color = clip_aabb(variance_aabb.min, variance_aabb.max, history_ycocg);
    rectified_color = mul(utils::YCoCg2RGB, rectified_color);

    alpha = clamp(alpha, 0.0, 1.0);

    history_color = lerp(history_color, rectified_color, 1.0);

    color.rgb = lerp(history_color, color.rgb, alpha);
    color.rgb = luma_weight_inverse(color.rgb);

    return max(color, 0);
}

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0) : SV_Target
{
    return taa(tex_coord, u_taa.camera);
}
