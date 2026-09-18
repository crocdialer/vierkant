module ray_common;

import "../utils/packed_vertex.slang";
import "../utils/octahedral_map.slang";
import "../utils/color.slang";
import "../utils/constants.slang";
import "pixel_data.slang";
import "types.slang";

namespace ray
{

public static const float EPS = 1e-4;
public static const float PDF_EPS = 1e-3;

public static const float DEFAULT_TMAX = 10000.0f;

//! path regularization: bounces below this pdf-threshold mark the path
public static const float REGULARIZE_PDF_THRESHOLD = 1e3;

//! largest of the three channels
public float max3(float3 v) { return v.x > v.y ? max(v.x, v.z) : max(v.y, v.z); }

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

//! light-bodies count down from here, scene-entries count up from zero, 0xFFFF is a miss.
//! the two only meet when entries + lights exceed the 16 bits entity_index provides
public static const uint LIGHT_ENTITY_INDEX_TOP = 0xFFFE;

//! closed-form ray-intersection with an analytic light body
public struct light_hit_t
{
    //! hit distance, <= 0 marks a miss
    public float t;

    //! cosine at the light surface (facing the ray origin)
    public float cos_light;
};

//! intersect a ray with an analytic light body. front-facing surfaces only: back sides are
//! transparent, consistent with the one-sided black NEE samples. non-area types never hit
public light_hit_t intersect_light(light_t l, float3 P, float3 dir)
{
    light_hit_t ret;
    ret.t = 0.0;
    ret.cos_light = 0.0;

    switch(l.type)
    {
        case LightType::Sphere:
        {
            float3 to_center = l.position - P;
            float d2 = dot(to_center, to_center);
            float r2 = l.size_x * l.size_x;

            // inside the sphere: no hit (matches the black NEE sample)
            if(d2 <= r2) { break; }
            float b = dot(dir, to_center);
            float disc = b * b - d2 + r2;
            if(disc <= 0.0) { break; }
            float t = b - sqrt(disc);
            if(t <= 0.0) { break; }
            ret.t = t;
            ret.cos_light = dot((P + t * dir - l.position) / l.size_x, -dir);
            break;
        }
        case LightType::Rect:
        case LightType::Disk:
        {
            float cos_light = dot(l.direction, -dir);
            if(cos_light <= 0.0) { break; }
            float t = dot(l.position - P, l.direction) / -cos_light;
            if(t <= 0.0) { break; }

            float3 q = P + t * dir - l.position;
            if(l.type == LightType::Rect)
            {
                float3 bitangent = cross(l.direction, l.tangent);
                if(abs(dot(q, l.tangent)) > l.size_x || abs(dot(q, bitangent)) > l.size_y) { break; }
            }
            else if(dot(q, q) > l.size_x * l.size_x) { break; }
            ret.t = t;
            ret.cos_light = cos_light;
            break;
        }
        case LightType::Tube:
        {
            // nearest mantle intersection (quadratic in the plane perpendicular to the axis)
            float3 o = P - l.position;
            float3 d_perp = dir - l.direction * dot(dir, l.direction);
            float3 o_perp = o - l.direction * dot(o, l.direction);
            float A = dot(d_perp, d_perp);
            float B = dot(o_perp, d_perp);
            float C = dot(o_perp, o_perp) - l.size_x * l.size_x;
            float disc = B * B - A * C;

            // parallel to the axis, miss, or P inside the mantle radius (backfacing hit)
            if(A <= EPS || disc <= 0.0 || C <= 0.0) { break; }
            float t = (-B - sqrt(disc)) / A;
            if(t <= 0.0 || abs(dot(o + t * dir, l.direction)) > l.size_y) { break; }

            // radial unit-normal at the hit
            float cos_light = dot((o_perp + t * d_perp) / l.size_x, -dir);
            if(cos_light <= 0.0) { break; }
            ret.t = t;
            ret.cos_light = cos_light;
            break;
        }
        default: break;
    }
    return ret;
}

//! cull-mask for rays that must not see the light-bodies instance, which the host tags with
//! RayBuilder::light_instance_mask (0x80). ray_cast() needs it: it calls Proceed() once, which only
//! completes traversal while nothing procedural can turn up
public static const uint GEOMETRY_MASK = 0x7F;

//! maximum number of nested media a path can track (mirrored host-side in vierkant/media.hpp)
public static const uint MAX_MEDIA_STACK_SIZE = 4;

public enum MediaOp : uint
{
    NoOp = 0,
    Enter = 1,
    Leave = 2,
};

public struct media_t
{
    public float3 sigma_s = float3(0);
    public float ior = 1.0;
    public float3 sigma_a = float3(0);
    public float phase_g = 0.0;

    //! emitted radiance Le. volumetric emission is sigma_a * Le, so it requires absorption
    public float3 emission = float3(0);

    //! scene-entry of the volume this medium came from, MAX_UINT16 when it has none
    public uint entry_index = utils::MAX_UINT16;
};

//! media_t in the payload's storage-form: sigma_s/sigma_a/emission as rgb9e5, ior + phase_g as half2.
//! 20B instead of 48B - used for the raygen media-stack, which is live across every TraceRay.
public struct packed_media_t
{
    public uint sigma_s;
    public uint sigma_a;
    public uint emission;
    public uint ior_phase;

    //! scene-entry of the volume this medium came from, MAX_UINT16 when it has none
    public uint entry_index;

    public float ior() { return f16tof32(ior_phase & 0xFFFFu); }
};

//! vacuum/air in packed form: pack_rgb9e5(0) is 0, f32tof16(1.0) is 0x3C00. an empty media-stack
//! resolves to this, so air never occupies a slot
public static const packed_media_t VACUUM_MEDIA = {0u, 0u, 0u, 0x3C00u, utils::MAX_UINT16};

public packed_media_t pack_media(media_t m)
{
    packed_media_t p;
    p.sigma_s = utils::pack_rgb9e5(m.sigma_s);
    p.sigma_a = utils::pack_rgb9e5(m.sigma_a);
    p.emission = utils::pack_rgb9e5(m.emission);
    p.ior_phase = f32tof16(m.ior) | (f32tof16(m.phase_g) << 16);
    p.entry_index = m.entry_index;
    return p;
}

public media_t unpack_media(packed_media_t p)
{
    media_t m;
    m.sigma_s = utils::unpack_rgb9e5(p.sigma_s);
    m.sigma_a = utils::unpack_rgb9e5(p.sigma_a);
    m.emission = utils::unpack_rgb9e5(p.emission);
    m.ior = f16tof32(p.ior_phase & 0xFFFFu);
    m.phase_g = f16tof32(p.ior_phase >> 16);
    m.entry_index = p.entry_index;
    return m;
}

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

    //! projector-cookie uv-scale. Spot: 1/tan(outer_cone_angle)
    public float2 uv_scale;

    //! index into the bound texture-array, 0 marks 'no cookie'
    public uint texture_index;

    //! explicit padding: the buffer-reference array-stride follows the declared members, without
    //! rounding up to the struct-alignment. must stay in sync with vierkant::light_t::pad
    public uint pad;
};

//! one bucket of a Vose alias-table, picking a light in O(1) from two uniform draws
public struct light_alias_bin_t
{
    //! index picked when the threshold-draw fails
    public uint alias;

    //! probability of keeping this bin's own index instead of 'alias'
    public float threshold;

    //! selection-probability of this bin's own index. needed for the MIS pdf-lookup
    public float prob;
};

//! simple struct to group rayhit-parameters
public struct payload_t
{
    //! the ray that generated this payload
    public Ray ray;

    //! ray-cone spread_angle (lo) + width (hi), half2
    public uint packed_cone;

    //! rng-seed
    public uint rng_state;

    //! packed: entity_index[15:0] | depth[23:16] | stop[24] | media_op[26:25] | regularize[27] | caustic_path[28]
    public uint flags;

    //! worldspace position
    public float3 position;

    //! worldspace normal, oct-encoded snorm_2x16
    public uint packed_normal;

    //! primary-hit surface albedo (material color after texture sampling), RGBA8
    public uint packed_albedo;

    //! last_ior (lo) + disp_lambda (hi), half2
    public uint packed_ior_lambda;

    //! accumulated radiance along a path
    public float3 radiance;

    //! path throughput
    public float3 beta;

    //! solid-angle pdf of the bsdf/phase sample that spawned this ray; <= 0: no MIS competitor (camera ray, NEE off)
    public float prev_bsdf_pdf;

    //! media sigma_s / sigma_a / emission, rgb9e5
    public uint packed_sigma_s;
    public uint packed_sigma_a;
    public uint packed_emission;

    //! media ior (lo) + phase_g (hi), half2
    public uint packed_media_ior_phase;

    //! scene-entry of the volume the medium came from, MAX_UINT16 when it has none
    public uint media_entry_index;

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

    //! refractive-caustic marker: a specular transmission event has occurred since the last non-specular
    //! vertex. cleared at every non-specular bounce. used to drop hit-side light adds (no refractive caustics)
    public property bool caustic_path
    {
        get { return (flags & (1u << 28)) != 0u; }
        set { flags = newValue ? flags | (1u << 28) : flags & ~(1u << 28); }
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

    //! used to determine texture-LoD
    public property RayCone cone
    {
        get
        {
            RayCone c;
            c.spread_angle = f16tof32(packed_cone & 0xFFFFu);
            c.width = f16tof32(packed_cone >> 16);
            return c;
        }
        set { packed_cone = f32tof16(newValue.spread_angle) | (f32tof16(newValue.width) << 16); }
    }

    public property float last_ior
    {
        get { return f16tof32(packed_ior_lambda & 0xFFFFu); }
        set { packed_ior_lambda = (packed_ior_lambda & 0xFFFF0000u) | f32tof16(newValue); }
    }

    //! locked dispersion wavelength in nm (< 0 = unlocked); set on first dispersive transmission
    public property float disp_lambda
    {
        get { return f16tof32(packed_ior_lambda >> 16); }
        set { packed_ior_lambda = (packed_ior_lambda & 0xFFFFu) | (f32tof16(newValue) << 16); }
    }

    //! media in storage-form, moved without a decode/encode roundtrip
    public property packed_media_t packed_media
    {
        get
        {
            packed_media_t p;
            p.sigma_s = packed_sigma_s;
            p.sigma_a = packed_sigma_a;
            p.emission = packed_emission;
            p.ior_phase = packed_media_ior_phase;
            p.entry_index = media_entry_index;
            return p;
        }
        set
        {
            packed_sigma_s = newValue.sigma_s;
            packed_sigma_a = newValue.sigma_a;
            packed_emission = newValue.emission;
            packed_media_ior_phase = newValue.ior_phase;
            media_entry_index = newValue.entry_index;
        }
    }

    public property media_t media
    {
        get { return unpack_media(packed_media); }
        set { packed_media = pack_media(newValue); }
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

    //! number of seeded entries in 'camera_media', outermost first. >0 also disables back-face culling
    public uint camera_media_count;

    //! debug: force a single direct-light estimator
    public MisMode mis_mode;

    //! drop hit-side light adds on refractive-caustic paths ("no refractive caustics")
    public bool suppress_refractive_caustics;

    //! maximum tolerable smear, as a fraction of image-height. 0 -> no per-pixel drift-limit
    public float max_accumulation_drift;

    //! number of direct lights
    public uint num_lights;

    //! number of Directional lights, stored first in the light-array
    public uint num_directional_lights;

    //! russian-roulette beta-threshold slope per bounce, 0: off
    public float rr_threshold;

    //! NEE probability = beta / (nee_beta_scale * depth), clamped. 0: off
    public float nee_beta_scale;

    //! 16byte alignment
    private int pad1, pad2, pad3;
};

public struct camera_ubo_t
{
    public float4x4 projection_view;
    public float4x4 projection_inverse;
    public float4x4 view_inverse;

    //! projection-view of the most recently traced frame, used to measure reprojection-drift
    public float4x4 prev_projection_view;
    public float fov;
    public float aperture;
    public float focal_distance;
    public bool ortho;
};

public struct trace_data_t
{
    public trace_params_t params;
    public camera_ubo_t camera;
    //! media the camera is submerged in, outermost first. seeds the path's media-stack
    public media_t camera_media[MAX_MEDIA_STACK_SIZE];

    // device pointers
    public Ptr<utils::packed_vertex_t, Access.Read> *vertex_buffers;
    public Ptr<uint32_t, Access.Read> *index_buffers;
    public Ptr<entry_t, Access.Read> entries;
    public Ptr<material_t, Access.Read> materials;
    public Ptr<light_t, Access.Read> lights;
    public Ptr<light_alias_bin_t, Access.Read> light_alias_table;
    public pixel_data_t *out_pixels;
};

}// namespace ray
