module packed_vertex;

import octahedral_map;

namespace utils
{

//! Vertex defines the layout for a vertex-struct
public struct Vertex
{
    public float3 position;
    public float2 tex_coord;
    public float3 normal;
    public float3 tangent;
};

public struct packed_vertex_t
{
    float pos_x, pos_y, pos_z;
    uint normal;
    uint tangent;
    float16_t texcoord_x, texcoord_y;
};

public struct bone_vertex_data_t
{
    public uint16_t index_x, index_y, index_z, index_w;

    //! weights are [0..1] as float16_t
    public float16_t weight_x, weight_y, weight_z, weight_w;
};

public Vertex unpack(packed_vertex_t v)
{
    Vertex ret;
    ret.position = float3(v.pos_x, v.pos_y, v.pos_z);
    ret.tex_coord = float2(v.texcoord_x, v.texcoord_y);
    ret.normal = utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(v.normal));
    ret.tangent = utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(v.tangent));
    return ret;
}

public packed_vertex_t pack(Vertex v)
{
    packed_vertex_t ret;
    ret.pos_x = v.position.x;
    ret.pos_y = v.position.y;
    ret.pos_z = v.position.z;

    ret.normal = utils::pack_snorm_2x16(utils::normalized_vector_to_octahedral_mapping(v.normal));
    ret.tangent = utils::pack_snorm_2x16(utils::normalized_vector_to_octahedral_mapping(v.tangent));

    ret.texcoord_x = float16_t(v.tex_coord.x);
    ret.texcoord_y = float16_t(v.tex_coord.y);
    return ret;
}

}
