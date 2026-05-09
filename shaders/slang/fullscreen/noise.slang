import "../utils/simplex.slang";

struct params_t
{
    float2 scale;
    float seed;
};

[[shader("pixel")]]
float4 fragment_main(ConstantBuffer<params_t> params, float2 tex_coord : TexCoord0) : SV_Target
{
    // Map 2D UV onto a 4D hypertorus (product of two unit circles) for seamless tiling.
    // u traces circle₁ in (x,y), v traces circle₂ in (z,w); both complete a full
    // revolution over [0,1], so cos/sin periodicity guarantees zero-seam wrapping.
    // Radius = scale keeps the sampling well outside a single simplex cell, which
    // hides the 4D lattice boundaries that appear when radius ≈ 1/τ.
    const float tau = 6.28318530717959;
    float2 ang = tau * tex_coord;
    float4 p = float4(cos(ang.x), sin(ang.x), cos(ang.y), sin(ang.y)) *
               float4(params.scale.x, params.scale.x, params.scale.y, params.scale.y) + params.seed;

    float n = utils::perlin(p) * 0.5 + 0.5;
    return float4(n, n, n, 1.0);
}
