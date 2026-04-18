[[vk::binding(0, 0)]] Sampler2D in_depth;
[[vk::binding(1, 0)]] RWTexture2D<float> out_depth;

struct ubo_t { float2 image_size; };
[[vk::binding(2, 0)]] ConstantBuffer<ubo_t> ubo;

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 coords = dispatchThreadID.xy;

    float depth = in_depth.SampleLevel((float2(coords) + float2(0.5)) / ubo.image_size, 0).x;

    out_depth[int2(coords)] = depth;
}
