module types;

import "../utils/transform.slang";

namespace ray
{

public struct aabb_t
{
    public float min_x, min_y, min_z;
    public float max_x, max_y, max_z;

    public float3 half_extents() { return float3(max_x - min_x, max_y - min_y, max_z - min_z) * 0.5f; }
};

//! entry_t holds properties for geometric entries with common attributes
public struct entry_t
{
    // per entry
    public float4x4 texture_matrix;
    public utils::transform_t transform;
    public utils::transform_t inv_transform;
    public aabb_t aabb;
    public uint material_index;
    public int vertex_offset;
    public uint base_index;

    // per mesh
    public uint buffer_index;

    int pad[2];
};

//! texture-type flag bits
public enum TextureTypeBits : uint
{
    Color = 0x001,
    Normal = 0x002,
    AoRoughMetal = 0x004,
    Emission = 0x008,
    Displacement = 0x010,
    Thickness = 0x020,
    Transmission = 0x040,
    Clearcoat = 0x080,
    SheenColor = 0x100,
    SheenRoughness = 0x200,
    Iridescence = 0x400,
    Environment = 0x800,
};

//! blend-mode definitions
public enum BlendMode : uint
{
    Opaque = 0,
    Blend = 1,
    Mask = 2,
};

//! material_t groups all material-properties for path-tracing
public struct material_t
{
    public float4 color;
    public float4 emission;
    public float metalness;
    public float roughness;
    public float transmission;
    public bool null_surface;
    public float3 attenuation_color;
    public float attenuation_distance;
    public float ior;
    public float clearcoat;
    public float clearcoat_roughness;
    public float sheen_roughness;
    public float4 sheen_color;

    public float iridescence_strength;
    public float iridescence_ior;
    public float2 iridescence_thickness_range;

    public uint albedo_index;
    public uint normalmap_index;
    public uint emission_index;
    public uint ao_rough_metal_index;

    public uint transmission_index;
    public uint texture_type_flags;
    public BlendMode blend_mode;
    public float alpha_cutoff;

    public bool two_sided;
    public float phase_asymmetry_g;
    public float scattering_ratio;

    int pad[1];
};

}// namespace ray
