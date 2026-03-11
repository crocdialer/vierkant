import "utils/transform.slang";
import "utils/packed_vertex.slang";
import "utils/slerp.slang";

struct skin_compute_params_t
{
    utils::packed_vertex_t *vertex_in;
    utils::bone_vertex_data_t *bone_vertex_data_in;
    utils::transform_t *bones_in;
    utils::packed_vertex_t *vertex_out;
    uint num_vertices;
};

struct morph_compute_params_t
{
    utils::packed_vertex_t *vertex_in;
    utils::packed_vertex_t *vertex_out;
    utils::Vertex *morph_vertex_in;
    uint num_vertices;
    uint morph_count;
    float weights[64];
};

[shader("compute")]
[numthreads(64, 1, 1)]
void skin_compute(uint3 dispatchThreadID: SV_DispatchThreadID,
                  StructuredBuffer<skin_compute_params_t> params_buf)
{
    skin_compute_params_t params = params_buf[0];
    uint gid = dispatchThreadID.x;
    if(gid >= params.num_vertices) { return; }
    utils::Vertex v = utils::unpack(params.vertex_in[gid]);

    float4 bone_weights = float4(params.bone_vertex_data_in[gid].weight_x, params.bone_vertex_data_in[gid].weight_y,
                                 params.bone_vertex_data_in[gid].weight_z, params.bone_vertex_data_in[gid].weight_w);
    uint4 bone_ids = uint4(params.bone_vertex_data_in[gid].index_x, params.bone_vertex_data_in[gid].index_y,
                           params.bone_vertex_data_in[gid].index_z, params.bone_vertex_data_in[gid].index_w);

    float3 new_pos = float3(0);
    float3 new_normal = float3(0);
    float3 new_tangent = float3(0);

    for(int i = 0; i < 4; i++)
    {
        new_pos += utils::apply_transform(params.bones_in[bone_ids[i]], v.position) * bone_weights[i];
        new_normal += utils::apply_rotation(params.bones_in[bone_ids[i]], v.normal) * bone_weights[i];
        new_tangent += utils::apply_rotation(params.bones_in[bone_ids[i]], v.tangent) * bone_weights[i];
    }

    v.position = new_pos;
    v.normal = normalize(new_normal);
    v.tangent = normalize(new_tangent);

    // write transformed+packed vertex
    params.vertex_out[gid] = utils::pack(v);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void morph_compute(uint3 dispatchThreadID: SV_DispatchThreadID,
                   StructuredBuffer<morph_compute_params_t> params_buf)
{
    morph_compute_params_t params = params_buf[0];
    uint gid = dispatchThreadID.x;
    if(gid >= params.num_vertices) { return; }

    utils::Vertex v = utils::unpack(params.vertex_in[gid]);
    float3 new_normal = float3(0);
    float3 new_tangent = float3(0);

    // apply morph-targets
    for(uint i = 0; i < params.morph_count; ++i)
    {
        uint morph_index = i * params.num_vertices + gid;
        utils::Vertex morph_vertex = params.morph_vertex_in[morph_index];

        v.position += morph_vertex.position * params.weights[i];
        new_normal += utils::slerp(v.normal, v.normal + morph_vertex.normal, params.weights[i]);
        new_tangent += utils::slerp(v.tangent, v.tangent + morph_vertex.tangent, params.weights[i]);
    }
    v.normal = normalize(new_normal);
    v.tangent = normalize(new_tangent);

    // write transformed+packed vertex
    params.vertex_out[gid] = utils::pack(v);
}
