//! groups transformation matrices
struct matrix_struct_t
{
    mat4 modelview;
    mat4 projection;
    mat4 normal;
    mat4 texture;
};

struct mesh_draw_t
{
    matrix_struct_t current_matrices;
    matrix_struct_t last_matrices;
    uint mesh_index;
    uint material_index;
};

struct lod_t
{
    uint base_index;
    uint num_indices;
    uint base_meshlet;
    uint num_meshlets;
};

struct mesh_entry_t
{
    vec3 center;
    float radius;

    uint vertex_offset;
    uint vertex_count;

    uint lod_count;
    lod_t lods[8];
};

struct index_bundle_t
{
    uint mesh_draw_index;
    uint material_index;
    uint meshlet_index;
    uint triangle_index;
};

////! Vertex defines the layout for a vertex-struct
//struct Vertex
//{
//    vec3 position;
//    vec2 tex_coord;
//    vec3 normal;
//    vec3 tangent;
//};

//struct packed_vertex_t
//{
//    float pos_x, pos_y, pos_z;
//    uint8_t normal_x, normal_y, normal_z, normal_w;
//    uint16_t texcoord_x, texcoord_y;
//    uint8_t tangent_x, tangent_y, tangent_z, tangent_w;
//};

#define LOCATION_INDEX_BUNDLE 0
#define LOCATION_VERTEX_BUNDLE 4

//! blendmode definitions
#define BLEND_MODE_OPAQUE 0
#define BLEND_MODE_BLEND 1
#define BLEND_MODE_MASK 2

//! texture-type flag bits
#define TEXTURE_TYPE_COLOR 0x01
#define TEXTURE_TYPE_NORMAL 0x02
#define TEXTURE_TYPE_AO_ROUGH_METAL 0x04
#define TEXTURE_TYPE_EMISSION 0x08
#define TEXTURE_TYPE_DISPLACEMENT 0x10
#define TEXTURE_TYPE_THICKNESS 0x20
#define TEXTURE_TYPE_TRANSMISSION 0x40
#define TEXTURE_TYPE_CLEARCOAT 0x80
#define TEXTURE_TYPE_SHEEN_COLOR 0x100
#define TEXTURE_TYPE_SHEEN_ROUGHNESS 0x200
#define TEXTURE_TYPE_IRIDESCENCE = 0x400
#define TEXTURE_TYPE_ENVIRONMENT 0x800

//! material parameters
struct material_struct_t
{
    vec4 color;
    vec4 emission;
    float metalness;
    float roughness;
    float ambient;
    uint blend_mode;
    float alpha_cutoff;
    float iridescence_factor;
    float iridescence_ior;
    vec2 iridescence_thickness_range;
    uint base_texture_index;
    uint texture_type_flags;
};

//! some render-context passed as push-constant
struct render_context_t
{
    vec2 size;
    float time;
    uint random_seed;
    bool disable_material;
    bool debug_draw_ids;
    uint base_draw_index;
};

//! attribute locations in vierkant::Mesh
#define ATTRIB_POSITION 0
#define ATTRIB_COLOR 1
#define ATTRIB_TEX_COORD 2
#define ATTRIB_NORMAL 3
#define ATTRIB_TANGENT 4
#define ATTRIB_BONE_INDICES 5
#define ATTRIB_BONE_WEIGHTS 6

//! descriptorset-bindings in vierkant::Renderer
#define BINDING_VERTICES 0
#define BINDING_INDICES 1
#define BINDING_DRAW_COMMANDS 2
#define BINDING_MESH_DRAWS 3
#define BINDING_MATERIAL 4
#define BINDING_TEXTURES 5
#define BINDING_BONES 6
#define BINDING_PREVIOUS_BONES 7
#define BINDING_JITTER_OFFSET 8
#define BINDING_MORPH_TARGETS 9
#define BINDING_MORPH_PARAMS 10
#define BINDING_PREVIOUS_MORPH_PARAMS 11
#define BINDING_MESHLETS 12
#define BINDING_MESHLET_VERTICES 13
#define BINDING_MESHLET_TRIANGLES 14

//! combined indirect-draw struct
struct indexed_indirect_command_t
{
    //! VkDrawIndexedIndirectCommand
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;

    //! VkDrawMeshTasksIndirectCommandNV
    uint taskCount;
    uint firstTask;

    bool visible;
    uint object_index;
    uint base_meshlet;
    uint num_meshlets;
    uint count_buffer_offset;
    uint first_draw_index;
};

//! meshlet parameters
struct meshlet_t
{
    //! offsets within meshlet_vertices and meshlet_triangles
    uint vertex_offset;
    uint triangle_offset;

    //! number of vertices and triangles used in the meshlet
    uint vertex_count;
    uint triangle_count;

    //! bounding sphere (center, radius), useful for frustum and occlusion culling
    vec3 sphere_center;
    float sphere_radius;

    //! normal cone (axis, cutoff), useful for backface culling
    vec3 cone_axis;
    float cone_cutoff;
};