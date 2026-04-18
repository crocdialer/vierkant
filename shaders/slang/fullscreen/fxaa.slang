import "../utils/fxaa.slang";

[[vk::binding(0, 0)]] Sampler2D u_texture;

[[shader("pixel")]]
float4 fragment_main(float2 tex_coord : TexCoord0) : SV_Target
{
    return utils::fxaa(u_texture, tex_coord, utils::fxaa_default_settings);
}
