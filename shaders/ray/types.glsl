//! Vertex defines the layout for a vertex-struct
struct Vertex
{
    vec3 position;
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
};

//! Triangle groups triangle vertices
struct Triangle
{
    Vertex v0, v1, v2;
};

//! entry_t holds properties for geometric entries with common attributes
struct entry_t
{
    // per entry
    mat4 modelview;
    mat4 normal_matrix;
    uint material_index;
    int vertex_offset;
    uint base_index;

    // per mesh
    uint buffer_index;
};

//! material_t groups all material-properties
struct material_t
{
    vec4 color;
    vec4 emission;
    float metalness;
    float roughness;
    float transmission;
    float attenuation_distance;
    vec4 attenuation_color;
    float ior;
    float clearcoat;
    float clearcoat_roughness;
    float sheen_roughness;
    vec4 sheen_color;

    uint texture_index;
    uint normalmap_index;
    uint emission_index;
    uint ao_rough_metal_index;
};