import "../renderer/importance_sampling.slang";

static const float2 s_positions[3] = { float2(-1, -1), float2(-1, 3), float2(3, -1) };
static const float2 s_tex_coords[3] = { float2(0, 0), float2(0, 2), float2(2, 0) };

[[shader("vertex")]]
void vertex_main(uint vertexID: SV_VertexID, out float2 tex_coord, out float4 sv_position: SV_Position)
{
    tex_coord = s_tex_coords[vertexID];
    sv_position = float4(s_positions[vertexID], 0.0, 1.0);
}

[[shader("pixel")]]
float4 fragment_main(in float2 tex_coord) : SV_Target
{
    float roughness = tex_coord.y;
    float NoV = tex_coord.x;
    return float4(renderer::IntegrateBRDF(roughness, NoV), 0.0, 1.0);
}
