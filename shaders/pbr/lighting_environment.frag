#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "../renderer/types.glsl"

#define ONE_OVER_PI 0.31830988618379067153776752674503

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = 0) uniform ubo_t
{
    mat4 u_camera_transform;
    mat4 u_inverse_projection;
    int u_num_mip_levels;
    float u_env_light_strength;
};

#define ALBEDO 0
#define NORMAL 1
#define EMISSION 2
#define AO_ROUGH_METAL 3
#define BRDF_LUT 4

layout(binding = 1) uniform sampler2D u_sampler_2D[5];

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

    vec3 world_normal = mat3(u_camera_transform) * normal;
    vec3 world_reflect = mat3(u_camera_transform) * r;

    vec3 diffIr = sample_diffuse(u_sampler_cube[ENV_DIFFUSE], world_normal);

    float spec_mip_lvl = roughness * float(u_num_mip_levels - 1);

    vec3 specIr = textureLod(u_sampler_cube[ENV_SPEC], world_reflect, spec_mip_lvl).rgb;
    float NoV = clamp(dot(normal, v), 0.0, 1.0);

    vec2 brdfTerm = texture(u_sampler_2D[BRDF_LUT], vec2(NoV, roughness)).rg;

    const vec3 dielectricF0 = vec3(0.04);
    vec3 diffColor = albedo * (1.0 - metalness);// if it is metal, no diffuse color
    vec3 specColor = mix(dielectricF0, albedo, metalness);// since metal has no albedo, we use the space to store its F0

    // TODO: still in doubt about application of brdfTerm
    vec3 distEnvLighting = diffColor * diffIr + specIr * (specColor * (brdfTerm.x + brdfTerm.y));
    distEnvLighting *= u_env_light_strength * ambient_occlusion;

    return distEnvLighting;
}

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    // reconstruct position from depth
    vec3 clip_pos = vec3(gl_FragCoord.xy / context.size, gl_FragCoord.z);
    vec4 viewspace_pos = u_inverse_projection * vec4(2.0 * clip_pos.xy - 1, clip_pos.z, 1);
    vec3 position = viewspace_pos.xyz / viewspace_pos.w;

    // sample g-buffer
    vec4 color = texture(u_sampler_2D[ALBEDO], vertex_in.tex_coord);
    vec3 normal = normalize(texture(u_sampler_2D[NORMAL], vertex_in.tex_coord).xyz);
    vec3 ao_rough_metal = texture(u_sampler_2D[AO_ROUGH_METAL], vertex_in.tex_coord).rgb;
    vec3 emission = texture(u_sampler_2D[EMISSION], vertex_in.tex_coord).rgb;

    vec3 env_color = compute_enviroment_lighting(position, normal, color.rgb, ao_rough_metal.g, ao_rough_metal.b, ao_rough_metal.r);
    out_color = vec4(env_color + emission, 1.0);
}
