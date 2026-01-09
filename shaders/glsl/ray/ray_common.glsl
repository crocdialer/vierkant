#ifndef RAY_COMMON_GLSL
#define RAY_COMMON_GLSL

#define EPS 1e-4
#define PDF_EPS 1e-3

#include "../utils/packed_vertex.glsl"
#include "pixel_data.glsl"

// for material_t / entries
#include "types.glsl"

//! Triangle groups triangle vertices
struct Triangle
{
    Vertex v0, v1, v2;
};

struct Ray
{
    vec3 origin;
    vec3 direction;
    float tmin;
    float tmax;
};

struct RayCone
{
    float spread_angle;
    float width;
};

#define MISS_INDEX_DEFAULT 0

#define MEDIA_NO_OP 0
#define MEDIA_ENTER 1
#define MEDIA_LEAVE 2

struct media_t
{
    vec3 sigma_s;
    float ior;
    vec3 sigma_a;
    float phase_g;
};

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

    // worldspace position
    vec3 position;

    // terminate path
    bool stop;

    // worldspace normal
    vec3 normal;

    float last_ior;

    // accumulated radiance along a path
    vec3 radiance;

    // media-transition in/out/no-op
    uint media_op;

    // path throughput
    vec3 beta;

    // object/entity
    uint entity_index;

    media_t media;
};

struct trace_params_t
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
    mat4 projection_view;
    mat4 projection_inverse;
    mat4 view_inverse;
    float fov;
    float aperture;
    float focal_distance;
    bool ortho;
};

// array of vertex-buffers
layout(buffer_reference, scalar) readonly buffer VertexBuffer { packed_vertex_t v[]; };
layout(buffer_reference, scalar) readonly buffer VertexBufferArray { VertexBuffer v[]; };

// array of index-buffers
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uint v[]; };
layout(buffer_reference, scalar) readonly buffer IndexBufferArray { IndexBuffer v[]; };

// array of entries
layout(buffer_reference, scalar) readonly buffer EntryBuffer { entry_t v[]; };

// array of materials
layout(buffer_reference, scalar) readonly buffer MaterialBuffer{ material_t v[]; };

// array of output-pixels
layout(buffer_reference, scalar) writeonly buffer PixelBuffer{ pixel_data_t v[]; };


struct trace_data_t
{
    trace_params_t trace_params;

    camera_ubo_t cam;

    media_t cam_media;

    VertexBufferArray vertex_buffers;
    IndexBufferArray index_buffers;
    EntryBuffer entries;
    MaterialBuffer materials;
    PixelBuffer out_pixels;
};

#endif // RAY_COMMON_GLSL