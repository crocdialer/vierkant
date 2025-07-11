#ifndef PACKED_VERTEX_GLSL
#define PACKED_VERTEX_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types: require

#include "octahedral_map.glsl"

//! Vertex defines the layout for a vertex-struct
struct Vertex
{
    vec3 position;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
};

struct packed_vertex_t
{
    float pos_x, pos_y, pos_z;
    uint normal;
    uint tangent;
    float16_t texcoord_x, texcoord_y;
};

struct bone_vertex_data_t
{
    uint16_t index_x, index_y, index_z, index_w;

    //! weights are [0..1] as float16_t
    float16_t weight_x, weight_y, weight_z, weight_w;
};

Vertex unpack(packed_vertex_t v)
{
    Vertex ret;
    ret.position = vec3(v.pos_x, v.pos_y, v.pos_z);
    ret.tex_coord = vec2(v.texcoord_x, v.texcoord_y);
    ret.normal = octahedral_mapping_to_normalized_vector(unpack_snorm_2x16(v.normal));
    ret.tangent = octahedral_mapping_to_normalized_vector(unpack_snorm_2x16(v.tangent));
    return ret;
}

packed_vertex_t pack(Vertex v)
{
    packed_vertex_t ret;
    ret.pos_x = v.position.x;
    ret.pos_y = v.position.y;
    ret.pos_z = v.position.z;

    ret.normal = pack_snorm_2x16(normalized_vector_to_octahedral_mapping(v.normal));
    ret.tangent = pack_snorm_2x16(normalized_vector_to_octahedral_mapping(v.tangent));

    ret.texcoord_x = float16_t(v.tex_coord.x);
    ret.texcoord_y = float16_t(v.tex_coord.y);
    return ret;
}

#endif // PACKED_VERTEX_GLSL