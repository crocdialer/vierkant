import "../utils/transform.slang";
import "../utils/packed_vertex.slang";

module renderer;

namespace renderer
{

// Transformation data for the current and previous frame
public struct matrix_struct_t
{
    public float4x4 projection;
    public float4x4 texture;
    public utils::transform_t transform;
    private uint pad[2];
};

// Per-draw data
public struct mesh_draw_t
{
    public matrix_struct_t current_matrices;
    public matrix_struct_t last_matrices;
    public uint mesh_index;
    public uint material_index;
    public uint mesh_buffer_index;
    public uint16_t lod_index;
    public uint16_t lod_count;
};

// Level-of-detail data for a mesh
public struct lod_t
{
    public uint base_index;
    public uint num_indices;
    public uint base_meshlet;
    public uint num_meshlets;
};

public static const uint MAX_NUM_MESH_LODS = 8;

// Metadata for a mesh and its LODs
public struct mesh_entry_t
{
    public float3 center;
    public float radius;

    public uint vertex_offset;
    public uint vertex_count;
    public uint lod_count;
    private uint pad;
    public lod_t lods[MAX_NUM_MESH_LODS];
};

// bundle of draw-time indices (passed between shader stages)
public struct index_bundle_t
{
    public uint mesh_draw_index;
    public uint material_index;
    public uint meshlet_index;
    public uint lod_index;
    public uint lod_count;
};

// g-buffer interstage vertex-data
public struct g_buffer_vertex_data_t
{
    public float2 tex_coord;
    public float3 normal;

    //! tangent-direction in xyz, bitangent-handedness in w
    public float4 tangent;
    public float4 current_position;
    public float4 last_position;
};

// vertex-attribute location mapping
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

// material blend modes
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
    public bool two_sided;
    public uint texture_type_flags;
    public uint base_texture_index;

    //! explicit tail-padding: slang lays out buffer-reference structs naturally and does not round
    //! up to the struct-alignment, the host does. keeps the array-stride at 128 on both sides.
    private uint pad[3];
};

public enum DebugFlagBits
{
    NONE = 0x0,
    DRAW_ID = 0x1,
    LOD = 0x2
};

// push-constant payload, mirrors vierkant::Rasterizer::push_constants_t
public struct push_constants_t
{
    public float2 size;
    public float time;
    public uint random_seed;
    public bool disable_material;
    public uint debug_flags;
    public uint base_draw_index;
};

// Shader resource bindings
public enum ResourceBinding : uint
{
    RenderData = 0,
    Textures = 5,
    JitterOffset = 9,
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

public bool has_flag(uint flag_bits, uint flag) { return (flag_bits & flag) == flag; }

public void set_flag(inout uint flag_bits, uint flag, bool value)
{
    flag_bits = value ? (flag_bits | flag) : (flag_bits & ~flag);
}

// GPU draw command structure for both graphics and mesh shading
public struct indexed_draw_command_t
{
    // DrawIndexed-style parameters
    public uint index_count;
    public uint instance_count;
    public uint first_index;
    public int vertex_offset;
    public uint first_instance;

    // Mesh shader dispatch parameters
    public uint group_count_x;
    public uint group_count_y;
    public uint group_count_z;

    public DrawCommandFlags flags;
    public uint object_index;
    public uint base_meshlet;
    public uint num_meshlets;
    public uint meshlet_visibility_index;
    public uint count_buffer_offset;
    public uint first_draw_index;
    private uint pad;
};

// Compact representation of a meshlet
public struct meshlet_t
{
    public uint vertex_offset;
    public uint triangle_offset;

    public uint8_t vertex_count;
    public uint8_t triangle_count;

    //! normal cone, useful for backface culling (packed as snorm8)
    public int8_t cone_axis[3];
    public int8_t cone_cutoff;

    private uint8_t padding[2];

    //! bounding sphere (center, radius), useful for frustum and occlusion culling
    public float3 sphere_center;
    public float sphere_radius;
};

public static const uint MESHLET_MAX_VERTICES    = 64;
public static const uint MESHLET_MAX_TRIANGLES   = 84;
public static const uint MAX_MESH_WORKGROUP_SIZE = 128;

public struct mesh_task_payload_t
{
    public uint    meshlet_base_index;
    public uint8_t meshlet_delta_indices[MAX_MESH_WORKGROUP_SIZE];
    public uint    object_index;
};

// per-drawable buffer-addresses, indexed with mesh_draw_t::mesh_buffer_index
public struct mesh_buffers_t
{
    public Ptr<utils::packed_vertex_t, Access.Read> vertices;
    public Ptr<meshlet_t, Access.Read> meshlets;
    public Ptr<uint, Access.Read> meshlet_vertices;
    public Ptr<uint8_t, Access.Read> meshlet_triangles;
};

// frame-global buffers, provided as device-addresses in a single UBO (set 0, binding 0)
public struct render_data_t
{
    public Ptr<mesh_draw_t, Access.Read> mesh_draws;
    public Ptr<material_t, Access.Read> materials;
    public Ptr<mesh_buffers_t, Access.Read> mesh_buffers;
    public Ptr<indexed_draw_command_t, Access.Read> draw_commands;

    //! meshlet-visibility bitfield, written from the task-shader
    public uint *meshlet_visibilities;
};

}// namespace renderer