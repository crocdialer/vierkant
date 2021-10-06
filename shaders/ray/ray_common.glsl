#define FLOAT_MAX 3.402823466e+38
#define FLOAT_MIN 1.175494351e-38

#define EPS 0.0001

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

    // color-absorbtion per unit-length
    vec3 absorption;

    bool inside_media;
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