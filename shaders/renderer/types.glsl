#ifndef RENDERER_TYPES_GLSL
#define RENDERER_TYPES_GLSL

#include "../utils/transform.glsl"

//! groups transformation matrices
struct matrix_struct_t
{
    mat4 projection;
    mat4 texture;
    transform_t transform;
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
#define TEXTURE_TYPE_VOLUME_THICKNESS 0x20
#define TEXTURE_TYPE_TRANSMISSION 0x40
#define TEXTURE_TYPE_CLEARCOAT 0x80
#define TEXTURE_TYPE_SHEEN_COLOR 0x100
#define TEXTURE_TYPE_SHEEN_ROUGHNESS 0x200
#define TEXTURE_TYPE_IRIDESCENCE 0x400
#define TEXTURE_TYPE_IRIDESCENCE_THICKNESS 0x800
#define TEXTURE_TYPE_SPECULAR 0x1000
#define TEXTURE_TYPE_SPECULAR_COLOR 0x2000
#define TEXTURE_TYPE_ENVIRONMENT 0x4000

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
    float transmission;
    float ior;
    float attenuation_distance;
    vec4 attenuation_color;
    float clearcoat_factor;
    float clearcoat_roughness_factor;
    float iridescence_factor;
    float iridescence_ior;
    vec2 iridescence_thickness_range;
    uint base_texture_index;
    uint texture_type_flags;
    bool two_sided;
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
#define BINDING_BONE_VERTEX_DATA 6
#define BINDING_BONES 7
#define BINDING_PREVIOUS_BONES 8
#define BINDING_JITTER_OFFSET 9
#define BINDING_MORPH_TARGETS 10
#define BINDING_MORPH_PARAMS 11
#define BINDING_PREVIOUS_MORPH_PARAMS 12
#define BINDING_MESHLETS 13
#define BINDING_MESHLET_VERTICES 14
#define BINDING_MESHLET_TRIANGLES 15
#define BINDING_MESHLET_VISIBILITY 16
#define BINDING_DEPTH_PYRAMID 17

//! combined indirect-draw struct
struct indexed_indirect_command_t
{
    //! VkDrawIndexedIndirectCommand
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;

    //! VkDrawMeshTasksIndirectCommandEXT
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;

    bool visible;
    uint object_index;
    uint base_meshlet;
    uint num_meshlets;
    uint meshlet_visibility_index;
    uint count_buffer_offset;
    uint first_draw_index;
    uint pad;
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

#endif // RENDERER_TYPES_GLSL