[[vk::constant_id(0)]] const float luma_thresh_min = 0.95;
[[vk::constant_id(1)]] const float luma_thresh_max = 1.1;

[[vk::binding(0, 0)]] Sampler2D u_texture;

// NTSC luma formula
static const float3 to_luma = float3(0.299, 0.587, 0.114);

float4 thresh(float2 tex_coord)
{
    float4 color = max(u_texture.Sample(tex_coord), 0.0);
    float brightness = dot(color.rgb, to_luma);
    return lerp(float4(0.0, 0.0, 0.0, 1.0), color, smoothstep(luma_thresh_min, luma_thresh_max, brightness));
}

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0) : SV_Target
{
    // one tap per footprint-quadrant, combine with karis-average, weighting each
    // by 1/(1 + luma): a tap contributes at most color / (1 + luma(color))
    static const float2 tap_offsets[4] = {float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1)};

    // quadrant-centers of output-texel's footprint, in input-uv. derived from uv-gradient
    float2 quarter_texel = 0.25 * float2(abs(ddx(tex_coord).x), abs(ddy(tex_coord).y));

    float4 color = float4(0);
    float weight_sum = 0.0;

    for(uint i = 0; i < 4; ++i)
    {
        float4 tap = thresh(tex_coord + quarter_texel * tap_offsets[i]);
        float weight = 1.0 / (1.0 + dot(tap.rgb, to_luma));
        color += weight * tap;
        weight_sum += weight;
    }
    return color / weight_sum;
}
