//! groups transformation matrices
struct matrix_struct_t
{
    mat4 modelview;
    mat4 projection;
    mat4 normal;
    mat4 texture;
};

//! material parameters
struct material_struct_t
{
    vec4 color;
    vec4 emission;
    float metalness;
    float roughness;
    float ambient;
};

//! definition of a directional- or point-light
struct lightsource_t
{
    vec3 position;
    int type;
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
    int matrix_index;
    int material_index;
    vec2 size;
    vec2 clipping;
    float time;
    int disable_material;
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
#define BINDING_MATERIAL 1
#define BINDING_TEXTURES 2
#define BINDING_BONES 3

//! constant for maximum number of drawables per renderpass
const int MAX_NUM_DRAWABLES = 4096;

//! constant for maximum number of bones per mesh
const int MAX_NUM_BONES = 512;