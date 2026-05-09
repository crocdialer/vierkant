import "../utils/simplex.slang";

struct params_t
{
    float2 scale;
    float seed;
};

[[shader("pixel")]]
float4 fragment_main(params_t* params, float2 tex_coord : TexCoord0) : SV_Target
{
    float n = utils::simplex(float3(tex_coord * params.scale, params.seed)) * 0.5 + 0.5;
    return float4(n, n, n, 1.0);
}
