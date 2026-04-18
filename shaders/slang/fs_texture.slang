static const float2 s_positions[3] = {float2(-1, -1), float2(-1, 3), float2(3, -1)};
static const float2 s_tex_coords[3] = {float2(0, 0), float2(0, 2), float2(2, 0)};

enum TextureIndex { COLOR = 0, DEPTH = 1 };

[[vk::binding(0, 0)]] Sampler2D u_textures[2];

struct params_t
{
    float4 color;
    float depth_scale;
    float depth_bias;
};
[[vk::binding(1, 0)]] ConstantBuffer<params_t> u_params;

struct VertexOut
{
    float4 position : SV_Position;
    float2 tex_coord : TexCoord0;
};

struct DepthOutput
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

[[shader("vertex")]]
VertexOut vertex_main(uint vertex_id : SV_VertexID)
{
    VertexOut v;
    v.position = float4(s_positions[vertex_id], 0.0, 1.0);
    v.tex_coord = s_tex_coords[vertex_id];
    return v;
}

[[shader("pixel")]]
float4 fragment_main(VertexOut v)
{
    return u_params.color * u_textures[TextureIndex::COLOR].Sample(v.tex_coord);
}

[[shader("pixel")]]
DepthOutput fragment_depth_main(VertexOut v)
{
    DepthOutput o;
    o.color = u_params.color * u_textures[TextureIndex::COLOR].Sample(v.tex_coord);
    o.depth = u_params.depth_scale * u_textures[TextureIndex::DEPTH].Sample(v.tex_coord).x + u_params.depth_bias;
    return o;
}
