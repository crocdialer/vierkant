import "../utils/tonemap.slang";

enum TextureIndex { COLOR = 0, BLOOM = 1, MOTION = 2 };

[[vk::binding(0, 0)]] Sampler2D u_textures[3];

struct params_t
{
    float gamma;
    float exposure;
    float time_delta;
    float shutter_time;
    float motionblur_gain;
};
[[vk::binding(1, 0)]] ConstantBuffer<params_t> u_params;

float4 sample_motion_blur(Sampler2D tex, float2 coord, float2 motion)
{
    const uint max_num_taps = 8;
    uint tw, th;
    tex.GetDimensions(tw, th);
    uint num_taps = clamp(uint(length(motion * float2(tw, th)) + 0.5), 1u, max_num_taps);

    float4 color = tex.Sample(coord);
    for(uint i = 1; i < num_taps; ++i)
    {
        float2 offset = motion * (float(i) / (num_taps - 1) - 0.5);
        color += tex.Sample(coord + offset);
    }
    return color / num_taps;
}

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0) : SV_Target
{
    float gain = u_params.motionblur_gain * clamp(u_params.time_delta / u_params.shutter_time, 0.0, 2.0);
    float2 motion = gain * u_textures[TextureIndex::MOTION].Sample(tex_coord).rg;

    float4 hdr_color = sample_motion_blur(u_textures[TextureIndex::COLOR], tex_coord, motion);
    float3 bloom = u_textures[TextureIndex::BLOOM].Sample(tex_coord).rgb;

    float3 result = utils::tonemap_exposure(hdr_color.rgb + bloom, u_params.exposure);
    result = pow(result, float3(1.0 / u_params.gamma));
    return float4(result, hdr_color.a);
}
