import "../utils/color_ST2084_PQ.slang";

enum TextureIndex { SCENE = 0, UI = 1 };

//! output encoding, see vierkant::DisplayOutput::Encoding
enum Encoding { SDR = 0, HDR10 = 1 };

[[vk::binding(0, 0)]] Sampler2D u_textures[2];

struct params_t
{
    float exposure;
    float gamma;
    float peak_nits;
    float paper_white_nits;
    uint encoding;
};
[[vk::binding(1, 0)]] ConstantBuffer<params_t> u_params;

// ITU-R BT.2087, linear BT.709 -> linear BT.2020
static const float3x3 BT709_TO_BT2020 = float3x3(float3(0.6274, 0.3293, 0.0433),
                                                 float3(0.0691, 0.9195, 0.0114),
                                                 float3(0.0164, 0.0880, 0.8956));

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0) : SV_Target
{
    float4 scene = u_textures[TextureIndex::SCENE].Sample(tex_coord);

    // ui: sRGB-encoded, premultiplied alpha
    float4 ui = u_textures[TextureIndex::UI].Sample(tex_coord);

    // SDR: exposure-tonemap, gamma-encode, ui composited in gamma-space
    float3 sdr = pow(1.0 - exp(-scene.rgb * u_params.exposure), 1.0 / u_params.gamma);
    sdr = sdr * (1.0 - ui.a) + ui.rgb;

    if(u_params.encoding == Encoding::HDR10)
    {
        // exposure-tonemap with asymptote at peak, in units of paper-white
        float s = u_params.peak_nits / u_params.paper_white_nits;
        float3 color = s * (1.0 - exp(-scene.rgb * u_params.exposure / s));

        // pixels covered by ui use the SDR result, so translucent ui looks the same as in SDR
        if(ui.a > 0.0) { color = pow(sdr, u_params.gamma); }

        float3 nits = mul(BT709_TO_BT2020, color) * u_params.paper_white_nits;
        return float4(utils::color_st2084_pq_inverse_eotf(max(nits / 10000.0, 0.0)), 1.0);
    }
    return float4(sdr, scene.a * (1.0 - ui.a) + ui.a);
}
