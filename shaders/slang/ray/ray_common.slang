module ray_common;

import "../utils/packed_vertex.slang";
import "../utils/octahedral_map.slang";
import "../utils/color.slang";
import "pixel_data.slang";
import "types.slang";

namespace ray
{

public static const float EPS = 1e-4;
public static const float PDF_EPS = 1e-3;

public static const float DEFAULT_TMAX = 10000.0f;

//! Triangle groups triangle vertices
public struct Triangle
{
    public utils::Vertex v0, v1, v2;
};

public struct Ray
{
    public float3 origin;
    public float tmin;
    public float3 direction;
    public float tmax;
};

public struct RayCone
{
    public float spread_angle;
    public float width;
};

public static const uint MISS_INDEX_DEFAULT = 0;

public enum MediaOp : uint
{
    NoOp = 0,
    Enter = 1,
    Leave = 2,
};

public struct media_t
{
    public float3 sigma_s;
    public float ior;
    public float3 sigma_a;
    public float phase_g;

    //! emitted radiance Le. volumetric emission is sigma_a * Le, so it requires absorption
    public float3 emission;
    float pad;
};

public enum LightType : uint
{
    Omni = 0,
    Spot = 1,
    Directional = 2,

    //! reserved for emissive-triangle lights (later phase)
    Area = 3,

    // analytic area lights
    Rect = 4,
    Sphere = 5,
    Tube = 6,
    Disk = 7,
};

//! packed lightsource-data, matches host-side vierkant::light_t
public struct light_t
{
    public float3 position;
    public LightType type;
    public float3 color;
    public float intensity;
    public float3 direction;
    public float range;
    public float spot_angle_scale;
    public float spot_angle_offset;

    //! >0 on directional lights indicates a sun-style disc-light (apex angle in radians)
    public float angular_size;

    //! area-light extent: half-height (Rect) / half-length (Tube)
    public float size_y;

    //! area-light in-plane x-axis (Rect)
    public float3 tangent;

    //! area-light extent: radius (Sphere/Disk/Tube) / half-width (Rect)
    public float size_x;
};

//! simple struct to group rayhit-parameters
public struct payload_t
{
    //! the ray that generated this payload
    public Ray ray;

    //! used to determine texture-LoD
    public RayCone cone;

    //! rng-seed
    public uint rng_state;

    //! packed: entity_index[15:0] | depth[23:16] | stop[24] | media_op[26:25] | regularize[27]
    public uint flags;

    //! worldspace position
    public float3 position;

    //! worldspace normal, oct-encoded snorm_2x16
    public uint packed_normal;

    //! primary-hit surface albedo (material color after texture sampling), RGBA8
    public uint packed_albedo;

    public float last_ior;

    //! locked dispersion wavelength in nm (< 0 = unlocked); set on first dispersive transmission
    public float disp_lambda;

    //! accumulated radiance along a path
    public float3 radiance;

    //! path throughput
    public float3 beta;

    //! solid-angle pdf of the bsdf/phase sample that spawned this ray; <= 0: no MIS competitor (camera ray, NEE off)
    public float prev_bsdf_pdf;

    public media_t media;

    //! object/entity
    public property uint entity_index
    {
        get { return flags & 0xFFFFu; }
        set { flags = (flags & ~0xFFFFu) | (newValue & 0xFFFFu); }
    }

    //! current depth of path
    public property uint depth
    {
        get { return (flags >> 16) & 0xFFu; }
        set { flags = (flags & ~(0xFFu << 16)) | ((newValue & 0xFFu) << 16); }
    }

    //! terminate path
    public property bool stop
    {
        get { return (flags & (1u << 24)) != 0u; }
        set { flags = newValue ? flags | (1u << 24) : flags & ~(1u << 24); }
    }

    //! media-transition in/out/no-op
    public property MediaOp media_op
    {
        get { return MediaOp((flags >> 25) & 0x3u); }
        set { flags = (flags & ~(0x3u << 25)) | ((uint(newValue) & 0x3u) << 25); }
    }

    //! path regularization: sticky, set after the first low-pdf bounce
    public property bool regularize
    {
        get { return (flags & (1u << 27)) != 0u; }
        set { flags = newValue ? flags | (1u << 27) : flags & ~(1u << 27); }
    }

    //! worldspace normal
    public property float3 normal
    {
        get { return utils::octahedral_mapping_to_normalized_vector(utils::unpack_snorm_2x16(packed_normal)); }
        set { packed_normal = utils::pack_snorm_2x16(utils::normalized_vector_to_octahedral_mapping(newValue)); }
    }

    //! primary-hit surface albedo (material color after texture sampling)
    public property float3 albedo
    {
        get { return utils::color_cast(packed_albedo).rgb; }
        set { packed_albedo = utils::color_cast(float4(newValue, 1.0)); }
    }
};

//! debug: force a single direct-light estimator (verification: all modes must converge to the same image)
public enum MisMode : uint
{
    Full = 0,
    NeeOnly = 1,
    BsdfOnly = 2,
};

public struct trace_params_t
{
    //! current time since start in seconds
    public float time;

    //! sample-batch index
    public uint batch_index;

    //! samples per pixel
    public uint num_samples;

    //! maximum path depth
    public uint max_trace_depth;

    //! override albedo colors
    public bool disable_material;

    //! enable skybox/background rendering
    public bool draw_skybox;

    //! multiplier for radiance from environment
    public float environment;

    //! a provided random seed
    public uint random_seed;

    //! clamp indirect path-throughput
    public float max_path_beta;

    //! camera starts inside 'camera_media' (seed media-stack, disable back-face culling)
    public bool camera_inside_media;

    //! debug: force a single direct-light estimator
    public MisMode mis_mode;

    private int pad1;
};

public struct camera_ubo_t
{
    public float4x4 projection_view;
    public float4x4 projection_inverse;
    public float4x4 view_inverse;
    public float fov;
    public float aperture;
    public float focal_distance;
    public bool ortho;
};

public struct trace_data_t
{
    public trace_params_t params;
    public camera_ubo_t camera;
    public media_t camera_media;

    // device pointers
    public Ptr<utils::packed_vertex_t, Access.Read> *vertex_buffers;
    public Ptr<uint32_t, Access.Read> *index_buffers;
    public Ptr<entry_t, Access.Read> entries;
    public Ptr<material_t, Access.Read> materials;
    public pixel_data_t *out_pixels;
    public Ptr<light_t, Access.Read> lights;
    public uint num_lights;
};

}// namespace ray
