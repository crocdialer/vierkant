[[vk::constant_id(0)]] const float luma_thresh_min = 0.95;
[[vk::constant_id(1)]] const float luma_thresh_max = 1.1;

[[vk::binding(0, 0)]] Sampler2D u_texture;

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0) : SV_Target
{
    float4 color = max(u_texture.Sample(tex_coord), 0.0);

    // NTSC luma formula
    const float3 to_luma = float3(0.299, 0.587, 0.114);
    float brightness = dot(color.rgb, to_luma);

    return lerp(float4(0.0, 0.0, 0.0, 1.0), color, smoothstep(luma_thresh_min, luma_thresh_max, brightness));
}
