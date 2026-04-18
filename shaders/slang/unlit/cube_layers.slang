struct ubo_matrices_t
{
    float4x4 u_view_matrix[6];
    float4x4 u_model_matrix;
    float4x4 u_projection_matrix;
};
[[vk::binding(0, 0)]] ConstantBuffer<ubo_matrices_t> ubo;

static const float3 base_vertices[8] =
{
    float3(-0.5, -0.5,  0.5), // bottom left front  0
    float3( 0.5, -0.5,  0.5), // bottom right front 1
    float3( 0.5, -0.5, -0.5), // bottom right back  2
    float3(-0.5, -0.5, -0.5), // bottom left back   3
    float3(-0.5,  0.5,  0.5), // top left front     4
    float3( 0.5,  0.5,  0.5), // top right front    5
    float3( 0.5,  0.5, -0.5), // top right back     6
    float3(-0.5,  0.5, -0.5), // top left back      7
};

static const uint cube_indices[36] = {
    0, 1, 5, 5, 4, 0, // front
    1, 2, 6, 6, 5, 1, // right
    2, 3, 7, 7, 6, 2, // back
    3, 0, 4, 4, 7, 3, // left
    4, 5, 6, 6, 7, 4, // top
    3, 2, 1, 1, 0, 3, // bottom
};

[[shader("vertex")]]
void vertex_main(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID,
                 out float3 eye_vec, out uint layer: SV_RenderTargetArrayIndex,
                 out float4 sv_position: SV_Position)
{
    layer = instanceID;

    float4 tmp = mul(ubo.u_model_matrix, float4(base_vertices[cube_indices[vertexID]], 1.0));
    eye_vec = float3(tmp.x, -tmp.y, tmp.z);
    sv_position = mul(ubo.u_projection_matrix, mul(ubo.u_view_matrix[layer], tmp));
}
