#ifndef RAY_PIXEL_BUFFER_GLSL
#define RAY_PIXEL_BUFFER_GLSL

#include "../utils/octahedral_map.glsl"
#include "../utils/rgb_log_luv.glsl"

struct pixel_buffer_t
{
    vec3 radiance;
    float depth;

    // normal in octahedral encoding
    uint normal;

    uint object_id;

    uint pad[2];
};

pixel_buffer_t pack(vec3 radiance, vec3 normal, float depth, uint object_id)
{
    pixel_buffer_t ret;
    ret.radiance = radiance;
    ret.normal = pack_snorm_2x16(normalized_vector_to_octahedral_mapping(normal));
    ret.depth = depth;
    ret.object_id = object_id;
    return ret;
}

#endif// RAY_PIXEL_BUFFER_GLSL