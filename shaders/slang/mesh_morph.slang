import "utils/packed_vertex.slang";
import "utils/slerp.slang";

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
void main(uint3 dispatchThreadID: SV_DispatchThreadID, ConstantBuffer<morph_compute_params_t> params)
{
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
