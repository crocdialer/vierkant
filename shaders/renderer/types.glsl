#define PI 3.1415926535897932384626433832795
#define ONE_OVER_PI 0.31830988618379067153776752674503

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
    float gamma;
    float time;
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

//! specialization constant for maximum number of drawables per renderpass
layout (constant_id = 0) const int MAX_NUM_DRAWABLES = 4096;

//! specialization constant for maximum number of bones per mesh
layout (constant_id = 1) const int MAX_NUM_BONES = 512;