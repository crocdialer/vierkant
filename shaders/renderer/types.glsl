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
};

//! definition of a directional- or point-light
struct lightsource_t
{
    vec3 position;
    uint type;
    vec4 diffuse;
    vec4 ambient;
    vec3 direction;
    float intensity;
    float radius;
    float spotCosCutoff;
    float spotExponent;
    float quadraticAttenuation;
};

//! some render-context passed as push-constant
struct render_context_t
{
    vec2 size;
    vec2 clipping;
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
#define BINDING_MATRIX 0
#define BINDING_PREVIOUS_MATRIX 1
#define BINDING_MATERIAL 2
#define BINDING_TEXTURES 3
#define BINDING_BONES 4
#define BINDING_PREVIOUS_BONES 5
#define BINDING_JITTER_OFFSET 6

//! constant for maximum number of drawables per renderpass
const int MAX_NUM_DRAWABLES = 4096;

//! constant for maximum number of bones per mesh
const int MAX_NUM_BONES = 512;