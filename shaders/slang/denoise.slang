import "utils/constants.slang";
import "utils/octahedral_map.slang";
import "utils/color.slang";
import "ray/pixel_data.slang";

[[vk::binding(0)]]
StructuredBuffer<ray::pixel_data_t> in_pixel;

[[vk::binding(1)]]
RWTexture2D<float4> u_out_image;

[[vk::binding(2)]]
RWStructuredBuffer<float> out_depth;

[[vk::binding(3)]]
RWTexture2D<uint4> out_object_ids;

[[vk::binding(4)]]
RWTexture2D<float4> u_prev_pass;

struct push_constants_t
{
    uint2 size;
    bool denoise;
    uint step;
    uint pass_index;
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
        int2(-2,-1), int2(-1,-1), int2(0,-1), int2(1,-1), int2(2,-1),
        int2(-2, 0), int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0),
        int2(-2, 1), int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1),
        int2(-2, 2), int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2) };

    float colorPhi    = 1.0 / .9f;
    float normalPhi   = 1.0 / .3f;
    float positionPhi = 1.0f;

    uint buffer_idx = coords.y * constants.size.x + coords.x;

    // For pass 0, read radiance from the pixel buffer; subsequent passes read from the previous output
    float3 cval = (constants.pass_index == 0) ? in_pixel[buffer_idx].radiance.rgb : u_prev_pass[coords].rgb;

    float3 nval = utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(in_pixel[buffer_idx].normal));
    float depth_center = in_pixel[buffer_idx].depth;

    // Albedo demodulation: divide radiance by albedo before filtering to avoid blurring texture detail.
    // effective_albedo is clamped to avoid division by zero AND to avoid zeroing out sky pixels
    // (which have albedo = 0 since the miss shader never writes one).
    float3 albedo_center    = ray::unpack_albedo(in_pixel[buffer_idx].albedo);
    float3 effective_albedo = max(albedo_center, float3(0.01));
    float3 cval_demod       = cval / effective_albedo;
    float  lum_center       = dot(cval_demod, utils::LuminanceNTSC);

    float3 sum  = float3(0.0);
    float cum_w = 0.0;

    for(uint i = 0; i < 25; ++i)
    {
        int2 uv = clamp(coords + offset[i] * int(constants.step), int2(0), int2(constants.size - 1));
        buffer_idx = uv.y * constants.size.x + uv.x;

        float3 ctmp = (constants.pass_index == 0) ? in_pixel[buffer_idx].radiance.rgb : u_prev_pass[uv].rgb;

        // Luminance-based color weight — robust against HDR emissives
        float3 albedo_tmp  = ray::unpack_albedo(in_pixel[buffer_idx].albedo);
        float3 ctmp_demod  = ctmp / max(albedo_tmp, float3(0.01));
        float  lum_tmp     = dot(ctmp_demod, utils::LuminanceNTSC);
        float  lum_diff    = lum_center - lum_tmp;
        float  c_w         = min(exp(-(lum_diff * lum_diff) * colorPhi), 1.0);

        float3 ntmp = utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(in_pixel[buffer_idx].normal));
        float3 t    = nval - ntmp;
        float dist2 = dot(t, t);
        float n_w   = min(exp(-(dist2) * normalPhi), 1.0);

        // Depth edge-stopping
        float depth_sample = in_pixel[buffer_idx].depth;
        float p_w          = exp(-abs(depth_center - depth_sample) * positionPhi);

        float weight = c_w * n_w * p_w * kernel[i];
        sum   += ctmp_demod * weight;
        cum_w += weight;
    }

    // Re-modulate with clamped albedo — for sky pixels (albedo = 0) this is an identity transform
    return float4((sum / cum_w) * effective_albedo, 0.f);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    int2  coords     = int2(dispatchThreadID.xy);
    uint  buffer_idx = dispatchThreadID.y * constants.size.x + dispatchThreadID.x;

    // Depth and object-id outputs are written once on the first pass
    if(constants.pass_index == 0)
    {
        out_depth[buffer_idx] = in_pixel[buffer_idx].depth;
        out_object_ids[coords] = uint4(utils::MAX_UINT16 - in_pixel[buffer_idx].object_id, 0, 0, 0);
    }

    if(constants.denoise)
    {
        u_out_image[coords] = denoise(coords);
    }
    else
    {
        u_out_image[coords] = float4(in_pixel[buffer_idx].radiance, in_pixel[buffer_idx].alpha);
    }
}
