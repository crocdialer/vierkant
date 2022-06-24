//! groups transformation matrices
struct matrix_struct_t
{
    mat4 modelview;
    mat4 projection;
    mat4 normal;
    mat4 texture;
};

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
    bool disable_material;
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
#define BINDING_MATRIX 1
#define BINDING_PREVIOUS_MATRIX 2
#define BINDING_MATERIAL 3
#define BINDING_TEXTURES 4
#define BINDING_BONES 5
#define BINDING_PREVIOUS_BONES 6
#define BINDING_JITTER_OFFSET 7