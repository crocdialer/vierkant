import "utils/constants.slang";
import "utils/octahedral_map.slang";
import "ray/pixel_data.slang";

[[vk::binding(0)]]
StructuredBuffer<ray::pixel_data_t> in_pixel;

[[vk::binding(1)]]
RWTexture2D<float4> u_out_image;

[[vk::binding(2)]]
RWStructuredBuffer<float> out_depth;

[[vk::binding(3)]]
RWTexture2D<uint4> out_object_ids;

struct push_constants_t
{
    uint2 size;
    bool denoise;
};

[[vk::push_constant]]
push_constants_t constants;

//! edge-avoiding À-Trous Wavelet for denoising
float4 denoise(int2 coords)
{
    const float kernel[25] = {
        1.0/256.0, 1.0/64.0, 3.0/128.0, 1.0/64.0, 1.0/256.0,
        1.0/64.0,  1.0/16.0, 3.0/32.0,  1.0/16.0, 1.0/64.0,
        3.0/128.0, 3.0/32.0, 9.0/64.0,  3.0/32.0, 3.0/128.0,
        1.0/64.0,  1.0/16.0, 3.0/32.0,  1.0/16.0, 1.0/64.0,
        1.0/256.0, 1.0/64.0, 3.0/128.0, 1.0/64.0, 1.0/256.0 };

    const int2 offset[25] = {
        int2(-2,-2), int2(-1,-2), int2(0,-2), int2(1,-2), int2(2,-2),
        int2(-2,-1), int2(-1,-1), int2(0,-2), int2(1,-1), int2(2,-1),
        int2(-2, 0), int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0),
        int2(-2, 1), int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1),
        int2(-2, 2), int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2) };

    float colorPhi    = 1.0 / .9f;
    float normalPhi   = 1.0 / .3f;
    float positionPhi = 1.0 / .6f;

    uint buffer_idx = coords.y * constants.size.x + coords.x;
    float3 cval = in_pixel[buffer_idx].radiance.rgb;
    float3 nval = utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(in_pixel[buffer_idx].normal));

    float3 sum  = float3(0.0);
    float cum_w = 0.0;

    for(uint i = 0; i < 25; ++i)
    {
        int2 uv = clamp(coords + offset[i] * 2, int2(0), int2(constants.size - 1));
        buffer_idx = uv.y * constants.size.x + uv.x;

        // Color
        float3 ctmp = in_pixel[buffer_idx].radiance.rgb;
        float3 t    = cval - ctmp;
        float dist2 = dot(t, t);
        float c_w   = min(exp(-(dist2) * colorPhi), 1.0);

        float3 ntmp = utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(in_pixel[buffer_idx].normal));
        t     = nval - ntmp;
        dist2 = dot(t, t);
        float n_w = min(exp(-(dist2) * normalPhi), 1.0);

        float p_w = 1.0;

        float weight = c_w * n_w * p_w * kernel[i];
        sum   += ctmp * weight;
        cum_w += weight;
    }
    return float4(sum / cum_w, 0.f);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    int2  coords    = int2(dispatchThreadID.xy);
    uint  buffer_idx = dispatchThreadID.y * constants.size.x + dispatchThreadID.x;

    out_depth[buffer_idx] = in_pixel[buffer_idx].depth;

    if(constants.denoise)
    {
        u_out_image[coords] = denoise(coords);
    }
    else
    {
        u_out_image[coords] = float4(in_pixel[buffer_idx].radiance, in_pixel[buffer_idx].alpha);
    }

    out_object_ids[coords] = uint4(utils::MAX_UINT16 - in_pixel[buffer_idx].object_id, 0, 0, 0);
}
