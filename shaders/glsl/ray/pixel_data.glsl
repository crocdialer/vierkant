#ifndef RAY_PIXEL_DATA_GLSL
#define RAY_PIXEL_DATA_GLSL

#include "../utils/octahedral_map.glsl"
#include "../utils/rgb_log_luv.glsl"

struct pixel_data_t
{
    vec3 radiance;
    float alpha;
    float depth;

    // normal in octahedral encoding
    uint normal;

    uint object_id;

    uint pad[1];
};

pixel_data_t pack(vec3 radiance, float alpha, vec3 normal, float depth, uint object_id)
{
    pixel_data_t ret;
    ret.radiance = radiance;
    ret.alpha = alpha;
    ret.normal = pack_snorm_2x16(normalized_vector_to_octahedral_mapping(normal));
    ret.depth = depth;
    ret.object_id = object_id;
    return ret;
}

#endif// RAY_PIXEL_DATA_GLSL