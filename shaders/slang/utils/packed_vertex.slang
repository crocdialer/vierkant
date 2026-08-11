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

    //! glTF convention: tangent-direction in xyz, bitangent-handedness in w
    public float4 tangent;
};

public struct packed_vertex_t
{
    float pos_x, pos_y, pos_z;

    //! octahedral normal, tangent-angle and bitangent-handedness, see utils::pack_tangent_frame
    uint tangent_frame;

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

    float3 tangent;
    float handedness;
    utils::unpack_tangent_frame(v.tangent_frame, ret.normal, tangent, handedness);
    ret.tangent = float4(tangent, handedness);
    return ret;
}

//! unpack only position/tex_coord/normal - skips the tangent-frame's basis-construction and sincos
public Vertex unpack_no_tangent(packed_vertex_t v)
{
    Vertex ret;
    ret.position = float3(v.pos_x, v.pos_y, v.pos_z);
    ret.tex_coord = float2(v.texcoord_x, v.texcoord_y);
    ret.normal = utils::unpack_tangent_frame_normal(v.tangent_frame);
    ret.tangent = float4(0, 0, 0, 1);
    return ret;
}

public packed_vertex_t pack(Vertex v)
{
    packed_vertex_t ret;
    ret.pos_x = v.position.x;
    ret.pos_y = v.position.y;
    ret.pos_z = v.position.z;

    ret.tangent_frame = utils::pack_tangent_frame(v.normal, v.tangent.xyz, v.tangent.w);

    ret.texcoord_x = float16_t(v.tex_coord.x);
    ret.texcoord_y = float16_t(v.tex_coord.y);
    return ret;
}

}
