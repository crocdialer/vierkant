#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../renderer/types.glsl"
#include "../utils/color_ycc.glsl"

#define ONE_OVER_PI 0.31830988618379067153776752674503

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = 0) uniform ubo_t
{
    mat4 camera_transform;
    mat4 inverse_projection;
    float near;
    float far;
    int num_mip_levels;
    float env_light_strength;
} ubo;

#define ALBEDO 0
#define NORMAL 1
#define EMISSION 2
#define AO_ROUGH_METAL 3
#define MOTION 4
#define DEPTH 5
#define BRDF_LUT 6
#define HISTORY_COLOR 7
#define HISTORY_DEPTH 8

layout(binding = 1) uniform sampler2D u_sampler_2D[9];

#define ENV_DIFFUSE 0
#define ENV_SPEC 1
layout(binding = 2) uniform samplerCube u_sampler_cube[2];

vec3 sample_diffuse(in samplerCube diff_map, in vec3 normal)
{
    return texture(diff_map, normal).rgb * ONE_OVER_PI;
}

vec3 compute_enviroment_lighting(vec3 position, vec3 normal, vec3 albedo, float roughness, float metalness, float ambient_occlusion)
{
    vec3 v = normalize(position);
    vec3 r = normalize(reflect(v, normal));

    vec3 world_normal = mat3(ubo.camera_transform) * normal;
    vec3 world_reflect = mat3(ubo.camera_transform) * r;

    vec3 diffIr = sample_diffuse(u_sampler_cube[ENV_DIFFUSE], world_normal);

    float spec_mip_lvl = roughness * float(ubo.num_mip_levels - 1);

    vec3 specIr = textureLod(u_sampler_cube[ENV_SPEC], world_reflect, spec_mip_lvl).rgb;
    float NoV = clamp(dot(normal, v), 0.0, 1.0);

    vec2 brdfTerm = texture(u_sampler_2D[BRDF_LUT], vec2(NoV, roughness)).rg;

    const vec3 dielectricF0 = vec3(0.04);
    vec3 diffColor = albedo * (1.0 - metalness);// if it is metal, no diffuse color
    vec3 specColor = mix(dielectricF0, albedo, metalness);// since metal has no albedo, we use the space to store its F0

    // TODO: still in doubt about application of brdfTerm
    vec3 distEnvLighting = diffColor * diffIr + specIr * (specColor * (brdfTerm.x + brdfTerm.y));
    distEnvLighting *= ubo.env_light_strength * ambient_occlusion;

    return distEnvLighting;
}

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

float linear_depth(float depth)
{
    float n = ubo.near;
    float f = ubo.far;
    return (2.0 * n) / (f + n - depth * (f - n));
}

vec3 luma_weight(vec3 color)
{
    float luma = dot(LuminanceNTSC, color);
    return color / (1.0 + luma);
}

vec3 luma_weight_inverse(vec3 color)
{
    float luma = dot(LuminanceNTSC, color);
    return color / (1.0 - luma);
}

void main()
{
    float depth = texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x;

    // reconstruct position from depth
    vec3 clip_pos = vec3(gl_FragCoord.xy / context.size, depth);
    vec4 viewspace_pos = ubo.inverse_projection * vec4(2.0 * clip_pos.xy - 1, clip_pos.z, 1);
    vec3 position = viewspace_pos.xyz / viewspace_pos.w;

    // sample g-buffer
    vec4 albedo = texture(u_sampler_2D[ALBEDO], vertex_in.tex_coord);
    vec3 normal = normalize(texture(u_sampler_2D[NORMAL], vertex_in.tex_coord).xyz);
    vec3 ao_rough_metal = texture(u_sampler_2D[AO_ROUGH_METAL], vertex_in.tex_coord).rgb;
    vec3 emission = texture(u_sampler_2D[EMISSION], vertex_in.tex_coord).rgb;

    vec3 env_color = compute_enviroment_lighting(position, normal, albedo.rgb, ao_rough_metal.g, ao_rough_metal.b, ao_rough_metal.r);

    env_color += emission;

    // WIP: TAA implementation here
    vec2 history_coord = vertex_in.tex_coord - texture(u_sampler_2D[MOTION], vertex_in.tex_coord).rg;
    vec3 history_color = texture(u_sampler_2D[HISTORY_COLOR], history_coord).rgb;
    float history_depth = texture(u_sampler_2D[HISTORY_DEPTH], history_coord).x;

    float alpha = 0.1;

    // out of bounds sampling
    if(any(lessThan(history_coord, vec2(0))) || any(greaterThan(history_coord, vec2(1)))){ alpha = 1.0; }

    // reject based on depth
    const float depth_eps = 5.0e-3; // 0.00000006 = 1.0 / (1 << 24)
    alpha = abs(linear_depth(depth) - linear_depth(history_depth)) > depth_eps ? 1.0 : alpha;

    // TODO: reject and/or rectify based on color
    vec2 texel = 1.0 / textureSize(u_sampler_2D[ALBEDO], 0);
    vec3 min_color = vec3(1);
    vec3 max_color = vec3(0);

    // TODO: wrong place here, bring TAA into separate pass
//    // construct an AABB in color-space
//    for(int y = -1; y <= 1; ++y)
//    {
//        for(int x = -1; x <= 1; ++x)
//        {
//            vec3 color = luma_weight(texture(u_sampler_2D[ALBEDO], vertex_in.tex_coord + texel * vec2(x, y)).rgb;
//            min_color = min(min_color, color);
//            max_color = max(max_color, color);
//        }
//    }

//    env_color = mix(clamp(history_color, min_color, max_color), env_color, alpha);
    env_color = mix(vec3(1, 0, 0), env_color, alpha);

    out_color = vec4(env_color, 1.0);
}
