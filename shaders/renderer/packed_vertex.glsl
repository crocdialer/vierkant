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
    uint8_t normal_x, normal_y, normal_z, normal_w;
    float16_t texcoord_x, texcoord_y;
    uint8_t tangent_x, tangent_y, tangent_z, tangent_w;
};

Vertex unpack(packed_vertex_t v)
{
    Vertex ret;
    ret.position = vec3(v.pos_x, v.pos_y, v.pos_z);
    ret.tex_coord = vec2(v.texcoord_x, v.texcoord_y);
    ret.normal = vec3(int(v.normal_x), int(v.normal_y), int(v.normal_z)) / 127.0 - 1.0;
    ret.tangent = vec3(int(v.tangent_x), int(v.tangent_y), int(v.tangent_z)) / 127.0 - 1.0;
    return ret;
}