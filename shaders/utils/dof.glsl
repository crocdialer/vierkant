#ifndef DOF_GLSL
#define DOF_GLSL

#include "../utils/random.glsl"
#include "../utils/bsdf.glsl"

struct dof_params_t
{
    float focal_distance;
    float focal_length;
    float aperture;
    float sensor_width;
    float near;
    float far;
};

float linearize(float depth, float near, float far)
{
    // reverse+infite z
    return clamp(near / depth, near, far);
}

// average depth in diagonal-cross (5 taps)
float avg_depth(sampler2D depth_map, vec2 coord)
{
    const float spread = 1.5;

    vec2 tex_size = textureSize(depth_map, 0);
    vec2 texel = 1.0 / tex_size;

    // find avg depth in 3x3 neighbourhood
    float avg_depth = texture(depth_map, coord).x;

    const vec2[4] diagonal_cross = vec2[](vec2(-1), vec2(1), vec2(-1, 1), vec2(1, -1));

    for(uint o = 0; o < 4; ++o)
    {
        vec2 d_coord = coord + spread * texel * diagonal_cross[o];
        float d = texture(depth_map, d_coord).x;
        avg_depth += d;
    }
    avg_depth /= 5.0;
    return avg_depth;
}

vec4 depth_of_field(sampler2D color_map, sampler2D depth_map, vec2 coord, vec2 viewport_size, dof_params_t p)
{
    // init random number generator.
    uint rng_state = uint(viewport_size.x * gl_FragCoord.y + gl_FragCoord.x);
    //xxhash32(push_constants.random_seed, viewport_size.x * gl_FragCoord.y + gl_FragCoord.x);

    // scene depth calculation
    float depth = linearize(avg_depth(depth_map, coord), p.near, p.far);

    // normalized circle-of-confusion size [0..1]
    float circle_of_confusion_sz = abs((p.aperture * p.focal_length * (depth - p.focal_distance)) /
                                       (p.sensor_width * depth * (p.focal_length + p.focal_distance)));

    // stratified sampling of circle-of-confusion
    const uint num_taps = 13;
    const vec2 Xi = vec2(rnd(rng_state), rnd(rng_state));

    vec3 color = vec3(0);

    for (uint i = 0; i < num_taps; ++i)
    {
        const vec2 offset = 0.5 * circle_of_confusion_sz * sample_unit_disc(fract(Xi + Hammersley(i, num_taps)));
        color += texture(color_map, coord + offset).rgb;
    }
    color /= num_taps;
    return vec4(color, 1.0);
}

#endif // DOF_GLSL