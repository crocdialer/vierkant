#ifndef RAY_COMMON_GLSL
#define RAY_COMMON_GLSL

#define EPS 0.0001
#define PDF_EPS 0.0001

struct Ray
{
    vec3 origin;
    vec3 direction;
};

struct RayCone
{
    float spread_angle;
    float width;
};

#define MISS_INDEX_DEFAULT 0
#define MISS_INDEX_SHADOW 1

//! simple struct to groupt rayhit-parameters
struct payload_t
{
    // the ray that generated this payload
    Ray ray;

    // used to determine texture-LoD
    RayCone cone;

    // rng-seed
    uint rng_state;

    // current depth of path
    uint depth;

    // terminate path
    bool stop;

    // worldspace position
    vec3 position;

    // worldspace normal
    vec3 normal;

    // faceforward worldspace normal
    vec3 ff_normal;

    // accumulated radiance along a path
    vec3 radiance;

    // path throughput
    vec3 beta;

    // media refraction index
    float ior;

    // spectral attenuation per unit-length (sigma_s + sigma_a)
    vec3 sigma_t;

    bool transmission;
};

struct shadow_payload_t
{
    bool shadow;
};

struct push_constants_t
{
    //! current time since start in seconds
    float time;

    //! sample-batch index
    uint batch_index;

    //! spp - samples per pixel
    uint num_samples;

    //! spp - samples per pixel
    uint max_trace_depth;

    //! override albedo colors
    bool disable_material;

    //! enable skybox/background rendering
    bool draw_skybox;

    //! a provided random seed
    uint random_seed;
};

struct camera_ubo_t
{
    mat4 projection_inverse;
    mat4 view_inverse;
    float fov;
    float aperture;
    float focal_distance;
};

#endif // RAY_COMMON_GLSL