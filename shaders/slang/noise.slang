import "utils/psrdnoise.slang";

struct noise_params_t
{
    //! noise-frequency per axis
    float3 scale;

    //! rotation of the gradients in radians
    float alpha;

    //! translation of the noise-field
    float3 seed;

    //! non-zero: write the gradient to gba
    uint gradient;
};

[[vk::push_constant]] noise_params_t params;

// format-less, so any storage-capable color-format works. channels the format lacks are dropped
[[vk::image_format("unknown")]]
[[vk::binding(0, 0)]] RWTexture2D<float4> out_image_2d;

[[vk::image_format("unknown")]]
[[vk::binding(1, 0)]] RWTexture3D<float4> out_image_3d;

//! r: value in [0, 1], gba: gradient of r per unit texture-coordinate, or 0
float4 noise_texel(float3 uvw)
{
    // the lattice only wraps onto itself for integer periods, other axes stay unwrapped
    float3 period = select(params.scale == round(params.scale), params.scale, float3(0.0));

    float3 gradient;
    float n = utils::psrdnoise(uvw * params.scale + params.seed, period, params.alpha, gradient);
    float3 g = params.gradient != 0 ? 0.5 * params.scale * gradient : float3(0.0);
    return float4(0.5 * n + 0.5, g);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main_2d(uint3 tid: SV_DispatchThreadID)
{
    uint2 size;
    out_image_2d.GetDimensions(size.x, size.y);
    if(any(tid.xy >= size)) { return; }
    out_image_2d[tid.xy] = noise_texel(float3((float2(tid.xy) + 0.5) / float2(size), 0.0));
}

[shader("compute")]
[numthreads(4, 4, 4)]
void main_3d(uint3 tid: SV_DispatchThreadID)
{
    uint3 size;
    out_image_3d.GetDimensions(size.x, size.y, size.z);
    if(any(tid >= size)) { return; }
    out_image_3d[tid] = noise_texel((float3(tid) + 0.5) / float3(size));
}
