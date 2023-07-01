#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../renderer/types.glsl"

// rnd(state)
#include "../utils/random.glsl"

// bsdf utils
#include "../utils/bsdf.glsl"

#define DEPTH 0
#define NORMAL 1

layout (constant_id = 0) const int SSAO_KERNEL_SIZE = 9;
//layout (constant_id = 1) const float SSAO_RADIUS = 0.5;

layout(binding = 0, scalar) readonly buffer ParamsBuffer
{
    mat4 projection;
    mat4 inverse_projection;
    transform_t view_transform;
    float ssao_radius;
    uint random_seed;
} ubo;

layout(binding = 1) uniform sampler2D u_sampler_2D[2];

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(push_constant) uniform PushConstants{ render_context_t context; };

layout(location = 0) out float out_occlusion;

float screenspace_occlusion(sampler2D depth_sampler, vec2 coord, vec3 eye_normal, float ssao_radius, uint rng_state)
{
    float depth = texture(depth_sampler, coord).x;

    // reconstruct position from depth
    vec3 clip_pos = vec3(coord, depth);
    vec4 viewspace_pos = ubo.inverse_projection * vec4(2.0 * clip_pos.xy - 1, clip_pos.z, 1);
    vec3 position = viewspace_pos.xyz / viewspace_pos.w;

    // Calculate occlusion value
    float occlusion = 0.0;

    // remove banding
    const float bias = 0.00025;

    float sample_offset = 2 * rnd(rng_state) - 1;
    mat3 frame = local_frame(eye_normal);

    for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        // sample a direction from cosine distribution
        vec2 Xi = fract(Hammersley(i, SSAO_KERNEL_SIZE) + vec2(sample_offset));
        vec3 direction = frame * sample_hemisphere_cosine(Xi);

        // project
        vec3 new_pos = position + direction * ssao_radius;
        vec4 clip_coord = ubo.projection * vec4(new_pos, 1.0f);
        clip_coord /= clip_coord.w;
        vec2 sample_coord = clip_coord.xy * 0.5f + 0.5f;

        // sample depth, compare with current position/depth
        float sample_depth = texture(depth_sampler, sample_coord).x;
        float range_check = smoothstep(0.0f, 1.0f, ssao_radius / abs(position.z - new_pos.z));

        // check for negative curvature, weight with range
        occlusion += (sample_depth >= (depth + bias) ? 1.0f : 0.0f) * range_check;
    }
    occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
    return occlusion;
}

void main()
{
    // init random number generator.
    uint rng_state = xxhash32(ubo.random_seed, uint(context.size.x * gl_FragCoord.y + gl_FragCoord.x));

    // bring normal from world- to eye-coords
    vec3 eye_normal = texture(u_sampler_2D[NORMAL], vertex_in.tex_coord).xyz;
    eye_normal = apply_rotation(ubo.view_transform, eye_normal);

    out_occlusion = screenspace_occlusion(u_sampler_2D[DEPTH], vertex_in.tex_coord, eye_normal, ubo.ssao_radius,
                                          rng_state);
}