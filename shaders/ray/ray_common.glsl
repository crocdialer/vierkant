#include "../utils/random.glsl"

#define FLOAT_MAX 3.402823466e+38
#define FLOAT_MIN 1.175494351e-38

#define EPS 0.001

struct entry_t
{
    // per entry
    mat4 modelview;
    mat4 normal_matrix;
    uint material_index;
    uint base_vertex;
    uint base_index;

    // per mesh
    uint buffer_index;
};

struct Vertex
{
    vec3 position;
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
    vec3 tangent;
};

struct Ray
{
    vec3 origin;
    vec3 direction;
};

//! simple struct to groupt rayhit-parameters
struct payload_t
{
    // the ray that generated this payload
    Ray ray;

    // terminate path
    bool stop;

    // worldspace position
    vec3 position;

    // faceforward, worldspace normal
    vec3 normal;

    // accumulated radiance along a path
    vec3 radiance;

    // material absorbtion
    vec3 beta;

    // probability density
    float pdf;

    // media refraction index
    float ior;

    vec3 attenuation;

    float attenuation_distance;

    bool inside_media;
};

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

    float clearcoat_factor;
    float clearcoat_roughness_factor;
    float sheen_roughness;
    vec4 sheen_color;

    uint texture_index;
    uint normalmap_index;
    uint emission_index;
    uint ao_rough_metal_index;
};

struct push_constants_t
{
    //! current time since start in seconds
    float time;

    //! sample-batch index
    uint batch_index;

    //! override albedo colors
    bool disable_material;

    //! a provided random seed
    uint random_seed;
};

/*
 * Power heuristic often reduces variance even further for multiple importance sampling
 * Chapter 13.10.1 of pbrbook
 */
float powerHeuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

vec3 transmittance(vec3 attenuation_color, float attenuation_distance, float distance)
{
    return exp(log(attenuation_color) / attenuation_distance * distance);
}