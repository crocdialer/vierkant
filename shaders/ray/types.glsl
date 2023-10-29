#include "../utils/transform.glsl"

//! entry_t holds properties for geometric entries with common attributes
struct entry_t
{
    // per entry
    mat4 texture_matrix;
    transform_t transform;
    uint material_index;
    int vertex_offset;
    uint base_index;

    // per mesh
    uint buffer_index;
};

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

//! blendmode definitions
#define BLEND_MODE_OPAQUE 0
#define BLEND_MODE_BLEND 1
#define BLEND_MODE_MASK 2

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

    float iridescence_strength;
    float iridescence_ior;
    vec2 iridescence_thickness_range;

    uint texture_index;
    uint normalmap_index;
    uint emission_index;
    uint ao_rough_metal_index;

    uint texture_type_flags;

    uint blend_mode;
    float alpha_cutoff;
    bool two_sided;
};

//! medium_t groups medium-properties
struct medium_t
{
    //! spectral absorption/scattering coefficients and resulting attenuation
    //! sigma_t = sigma_a + sigma_s
    vec3 sigma_a, sigma_s;

    //! phase-function asymmetry parameter [-1, 1]
    float g;
};