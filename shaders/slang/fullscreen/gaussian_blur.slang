[[vk::constant_id(0)]] const uint num_taps = 9;

static const uint max_array_size = 4;
static const uint gaussian_array_size = num_taps / 4 + 1;
static const bool odd_num_samples = (num_taps / 2) % 2 == 0;

struct gaussian_ubo_t
{
    float4 offsets[max_array_size];
    float4 weights[max_array_size];
    float2 size;
};

[[vk::binding(0, 0)]] Sampler2D u_texture;
[[vk::binding(1, 0)]] ConstantBuffer<gaussian_ubo_t> u_settings;

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0) : SV_Target
{
    float4 color = float4(0);

    if(odd_num_samples)
    {
        color += u_texture.Sample(tex_coord) * u_settings.weights[0].x;
    }

    float2 texel = 1.0 / u_settings.size;

    for(uint i = odd_num_samples ? 1 : 0; i < gaussian_array_size; i++)
    {
        float2 offset = texel * u_settings.offsets[i].xy;
        color += u_texture.Sample(tex_coord + offset) * u_settings.weights[i].x;
        color += u_texture.Sample(tex_coord - offset) * u_settings.weights[i].x;
    }
    return color;
}
