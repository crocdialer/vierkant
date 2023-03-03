#extension GL_EXT_shader_explicit_arithmetic_types: require

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
    ret.normal = vec3(int(v.normal_x), int(v.normal_y), int(v.normal_z)) / 127.0 - 1.0;
    ret.tangent = vec3(int(v.tangent_x), int(v.tangent_y), int(v.tangent_z)) / 127.0 - 1.0;
    return ret;
}

packed_vertex_t pack(Vertex v)
{
    packed_vertex_t ret;
    ret.pos_x = v.position.x;
    ret.pos_y = v.position.y;
    ret.pos_z = v.position.z;

    ret.normal_x = uint8_t(v.normal.x * 127.f + 127.5f);
    ret.normal_y = uint8_t(v.normal.y * 127.f + 127.5f);
    ret.normal_z = uint8_t(v.normal.z * 127.f + 127.5f);
    ret.normal_w = uint8_t(0);

    ret.tangent_x = uint8_t(v.tangent.x * 127.f + 127.5f);
    ret.tangent_y = uint8_t(v.tangent.y * 127.f + 127.5f);
    ret.tangent_z = uint8_t(v.tangent.z * 127.f + 127.5f);
    ret.tangent_w = uint8_t(0);

    ret.texcoord_x = float16_t(v.tex_coord.x);
    ret.texcoord_y = float16_t(v.tex_coord.y);
    return ret;
}