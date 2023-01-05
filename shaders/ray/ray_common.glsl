#define FLOAT_MAX 3.402823466e+38
#define FLOAT_MIN 1.175494351e-38

#define EPS 0.0001

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

    // color-absorbtion per unit-length
    vec3 absorption;

    bool inside_media;
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