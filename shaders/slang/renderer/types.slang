#ifndef RENDERER_TYPES_SLANG
#define RENDERER_TYPES_SLANG

implementing renderer;

// Transformation data for the current and previous frame
public struct MatrixStruct
{
    public float4x4 projection;
    public float4x4 texture;
    public transform_t transform;
    uint pad[2];
};

// Per-draw data
public struct MeshDraw
{
    public MatrixStruct current_matrices;
    public MatrixStruct last_matrices;
    public uint mesh_index;
    public uint material_index;
    public uint vertex_buffer_index;
    uint pad[1];
};

// Level-of-detail data for a mesh
public struct lod_t
{
    uint base_index;
    uint num_indices;
    uint base_meshlet;
    uint num_meshlets;
};

#define MAX_NUM_MESH_LODS 8

// Metadata for a mesh and its LODs
public struct MeshEntry
{
    float3 center;
    float radius;

    uint vertex_offset;
    uint vertex_count;
    uint lod_count;
    uint pad;
    lod_t lods[MAX_NUM_MESH_LODS];
};

// Bundle of draw-time indices (passed between shader stages)
public struct IndexBundle
{
    public uint mesh_draw_index;
    public uint material_index;
    public uint meshlet_index;
    public uint triangle_index;
};

// Vertex output location mapping
public enum VertexLocation : uint
{
    IndexBundle = 0,
    VertexBundle = 4,
};

// Vertex attribute location mapping
public enum VertexAttrib : uint
{
    Position = 0,
    Color = 1,
    TexCoord = 2,
    Normal = 3,
    Tangent = 4,
    BoneIndices = 5,
    BoneWeights = 6,
};

// Material blend modes
public enum BlendMode : uint
{
    Opaque = 0,
    Blend = 1,
    Mask = 2,
};

// Texture type flags (used as bitfields)
public enum TextureType : uint
{
    Color = 0x01,
    Normal = 0x02,
    AoRoughMetal = 0x04,
    Emission = 0x08,
    Displacement = 0x10,
    VolumeThickness = 0x20,
    Transmission = 0x40,
    Clearcoat = 0x80,
    SheenColor = 0x100,
    SheenRoughness = 0x200,
    Iridescence = 0x400,
    IridescenceThickness = 0x800,
    Specular = 0x1000,
    SpecularColor = 0x2000,
    Environment = 0x4000,
};

// Material definition
public struct material_t
{
    public float4 color;
    public float4 emission;
    public float metalness;
    public float roughness;
    public float ambient;
    public BlendMode blend_mode;
    public float alpha_cutoff;
    public float transmission;
    public float ior;
    public float attenuation_distance;
    public float4 attenuation_color;
    public float clearcoat_factor;
    public float clearcoat_roughness_factor;
    public float iridescence_factor;
    public float iridescence_ior;
    public float2 iridescence_thickness_range;
    public uint base_texture_index;
    public uint texture_type_flags;
    public bool two_sided;
};

// Render-time context (usually passed via push constants)
public struct render_context_t
{
    float2 size;
    float time;
    uint random_seed;
    bool disable_material;
    bool debug_draw_ids;
    uint base_draw_index;
};

// Shader resource bindings
public enum ResourceBinding : uint
{
    Vertices = 0,
    Indices = 1,
    DrawCommands = 2,
    MeshDraws = 3,
    Material = 4,
    Textures = 5,
    BoneVertexData = 6,
    Bones = 7,
    PreviousBones = 8,
    JitterOffset = 9,
    MorphTargets = 10,
    MorphParams = 11,
    PreviousMorphParams = 12,
    Meshlets = 13,
    MeshletVertices = 14,
    MeshletTriangles = 15,
    MeshletVisibility = 16,
    DepthPyramid = 17
};

// Draw command flags (bitmask)
public enum DrawCommandFlags : uint
{
    Enabled = 0x01,
    Visible = 0x02,
    LateVisible = 0x04,
    Meshlets = 0x08,
};

public bool has_flag(uint flag_bits, uint flag)
{
    return (flag_bits & flag) == flag;
}

public void set_flag(inout uint flag_bits, uint flag, bool value)
{
    flag_bits = value ? (flag_bits | flag) : (flag_bits & ~flag);
}

// GPU draw command structure for both graphics and mesh shading
public struct IndexedIndirectCommand
{
    // DrawIndexed-style parameters
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;

    // Mesh shader dispatch parameters
    uint group_count_x;
    uint group_count_y;
    uint group_count_z;

    DrawCommandFlags flags;
    uint object_index;
    uint base_meshlet;
    uint num_meshlets;
    uint meshlet_visibility_index;
    uint count_buffer_offset;
    uint first_draw_index;
    uint pad;
};

// Compact representation of a meshlet
public struct Meshlet
{
    uint vertex_offset;
    uint triangle_offset;

    uint vertex_count;
    uint triangle_count;

    float3 sphere_center;
    float sphere_radius;

    float3 cone_axis;
    float cone_cutoff;
};

#endif // RENDERER_TYPES_SLANG