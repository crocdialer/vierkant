import "utils/packed_vertex.slang";
import "utils/barycentric_indexing.slang";

struct VkMicromapTriangleEXT
{
    uint dataOffset;
    uint16_t subdivisionLevel;
    uint16_t format;
};

struct micromap_params_t
{
    uint num_triangles;
    uint num_subdivisions;
    uint format; // VK_OPACITY_MICROMAP_FORMAT_2_STATE_EXT
    float alpha_cutoff;

    utils::packed_vertex_t *vertex_in;
    uint *index_in;
    uint8_t *micromap_data_out;
    VkMicromapTriangleEXT *micromap_triangle_out;
    uint *micromap_indices_out;
};

[[vk::binding(0)]]
StructuredBuffer<micromap_params_t> params_buf;

[[vk::binding(1)]]
Sampler2D u_albedo;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    micromap_params_t params = params_buf[0];
    uint gid = dispatchThreadID.x;
    if(gid >= params.num_triangles) { return; }

    uint3 indices = uint3(params.index_in[3 * gid],
                          params.index_in[3 * gid + 1],
                          params.index_in[3 * gid + 2]);

    utils::Vertex v0 = utils::unpack(params.vertex_in[indices[0]]);
    utils::Vertex v1 = utils::unpack(params.vertex_in[indices[1]]);
    utils::Vertex v2 = utils::unpack(params.vertex_in[indices[2]]);

    // for each micro-triangle
    const uint num_bits_per_micro_triangle = params.format;
    uint num_micro_triangles = utils::num_micro_triangles(params.num_subdivisions);
    uint data_tmp = 0;
    for(uint i = 0; i < num_micro_triangles; ++i)
    {
        // get barycentrics for index
        float2 micro_uv[3];
        utils::index2bary(i, params.num_subdivisions, micro_uv[0], micro_uv[1], micro_uv[2]);
        float2 center = (micro_uv[0] + micro_uv[1] + micro_uv[2]) / 3.0;
        float3 bary = float3(1.0f - center.x - center.y, center.x, center.y);

        // interpolate tex-coord
        float2 tex_coord = v0.tex_coord * bary.x + v1.tex_coord * bary.y + v2.tex_coord * bary.z;

        // sample opacity from texture
        float alpha = u_albedo.SampleLevel(tex_coord, 0).a;
        uint value = alpha > params.alpha_cutoff ? 1 : 0;

        uint next_bit = (i * num_bits_per_micro_triangle) % 8;
        data_tmp |= value << next_bit;

        if(i > 0 && next_bit == 0)
        {
            // one byte filled here, write out
            uint data_index = ((gid * num_micro_triangles + i) * num_bits_per_micro_triangle) / 8;
            params.micromap_data_out[data_index] = uint8_t(data_tmp);
            data_tmp = 0;
        }
    }

    // write triangles
    VkMicromapTriangleEXT triangle;
    triangle.dataOffset = (gid * num_micro_triangles * num_bits_per_micro_triangle) / 8;
    triangle.subdivisionLevel = uint16_t(params.num_subdivisions);
    triangle.format = uint16_t(params.format);
    params.micromap_triangle_out[gid] = triangle;

//    VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT = -1,
//    VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT = -2,
//    VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_TRANSPARENT_EXT = -3,
//    VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_UNKNOWN_OPAQUE_EXT = -4,
    params.micromap_indices_out[gid] = gid;
}
