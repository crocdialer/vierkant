#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_query : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../renderer/types.glsl"

// rnd(state)
#include "../utils/random.glsl"

// bsdf utils
#include "../utils/bsdf.glsl"

#define DEPTH 0
#define NORMAL 1

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1) uniform sampler2D u_sampler_2D[2];

layout(binding = 2, scalar) readonly buffer ParamsBuffer
{
    mat4 inverse_projection;
    transform_t camera_transform;
    uint num_rays;
    float max_distance;
} ubo;

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(push_constant) uniform PushConstants{ render_context_t context; };

layout(location = 0) out float out_occlusion;

float raytraced_occlusion(vec3 position, vec3 world_normal, float max_distance, uint num_rays, uint rng_state)
{
    const float tmin = 0.01, tmax = max_distance;

    float accumulated_ao = 0.f;
    float accumulated_factor = 0;

    float sample_offset = 2 * rnd(rng_state) - 1;
    mat3 frame = local_frame(world_normal);

    for(uint i = 0; i < num_rays; ++i)
    {
        vec2 Xi = fract(Hammersley(i, num_rays) + vec2(sample_offset));
        vec3 cos_dir = sample_hemisphere_cosine(Xi);
        vec3 direction = frame * cos_dir;

        rayQueryEXT query;
        rayQueryInitializeEXT(query, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, position, tmin, direction, tmax);
        rayQueryProceedEXT(query);
        float dist = max_distance;

        if (rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        {
            dist = rayQueryGetIntersectionTEXT(query, true);
        }

        float ao = min(dist, max_distance);
        float factor = 0.2 + 0.8 * cos_dir.z * cos_dir.z;// weighting not required since we sampled cosine(!?)
        accumulated_factor += factor;
        accumulated_ao += ao * factor;
    }
    accumulated_ao /= (max_distance * accumulated_factor);
    accumulated_ao *= accumulated_ao;
    return clamp(accumulated_ao, 0, 1);
}

void main()
{
    float depth = texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x;
    vec3 normal = texture(u_sampler_2D[NORMAL], vertex_in.tex_coord).xyz;

    // reconstruct position from depth
    vec3 clip_pos = vec3(gl_FragCoord.xy / context.size, depth);
    vec4 viewspace_pos = ubo.inverse_projection * vec4(2.0 * clip_pos.xy - 1, clip_pos.z, 1);
    vec3 position = apply_transform(ubo.camera_transform, viewspace_pos.xyz / viewspace_pos.w);

    out_occlusion = raytraced_occlusion(position, normal, ubo.max_distance, ubo.num_rays, context.random_seed);
}