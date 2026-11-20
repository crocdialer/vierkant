// test-shader for tests/TestHeapDescriptors.cpp.
// reads one descriptor of each type and writes the raw bits to out_values.

struct params_t
{
    //! number of descriptors in the variable-count binding
    uint num_textures;
};

[[vk::push_constant]] params_t params;

struct ubo_t
{
    uint4 value;
};

[[vk::binding(0, 0)]] ConstantBuffer<ubo_t> ubo;

[[vk::binding(1, 0)]] StructuredBuffer<uint4> ssbo;

[[vk::binding(2, 0)]] Texture2D<float4> sampled_image;

[[vk::binding(3, 0)]] RWTexture2D<float4> storage_image;

// same image, two sampler-states
[[vk::binding(4, 0)]] Sampler2D combined[2];

[[vk::binding(5, 0)]] Texture2D<float4> mip_view;

[[vk::binding(6, 0)]] Texture2DArray<float4> array_view;

[[vk::binding(7, 0)]] RWStructuredBuffer<uint4> out_values;

// set-path: inline uniform-block, heap-path: data inside the heap-block
[[vk::binding(8, 0)]] ConstantBuffer<ubo_t> inline_block;

[[vk::binding(9, 0)]] Texture2D<float4> textures[];

[shader("compute")]
[numthreads(1, 1, 1)]
void main()
{
    out_values[0] = ubo.value;
    out_values[1] = ssbo[0];
    out_values[2] = asuint(sampled_image.Load(int3(0, 0, 0)));
    out_values[3] = asuint(storage_image[int2(0, 0)]);

    // outside [0, 1]: clamp-to-edge and repeat pick different texels
    out_values[4] = asuint(combined[0].SampleLevel(float2(1.25, 0.5), 0));
    out_values[5] = asuint(combined[1].SampleLevel(float2(1.25, 0.5), 0));

    out_values[6] = asuint(mip_view.Load(int3(0, 0, 0)));
    out_values[7] = asuint(array_view.Load(int4(0, 0, 4, 0)));

    out_values[8] = inline_block.value;

    for(uint i = 0; i < params.num_textures; ++i) { out_values[9 + i] = asuint(textures[i].Load(int3(0, 0, 0))); }
}
