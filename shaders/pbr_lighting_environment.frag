#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "renderer/types.glsl"

#define ONE_OVER_PI 0.31830988618379067153776752674503

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = 0) uniform ubo_t
{
    mat4 u_camera_transform;
    int u_num_mip_levels;
    float u_env_light_strength;
};

#define ALBEDO 0
#define NORMAL 1
#define POSITION 2
#define EMISSION 3
#define AO_ROUGH_METAL 4
#define BRDF_LUT 5

layout(binding = 1) uniform sampler2D u_sampler_2D[6];

#define ENV_DIFFUSE 0
#define ENV_SPEC 1
layout(binding = 2) uniform samplerCube u_sampler_cube[2];

float map_roughness(float r)
{
    return mix(0.025, 0.975, r);
}

vec3 sample_diffuse(in samplerCube diff_map, in vec3 normal)
{
    return texture(diff_map, normal).rgb * ONE_OVER_PI;
}

vec3 compute_enviroment_lighting(vec3 position, vec3 normal, vec3 albedo, float roughness, float metalness, float aoVal)
{
    roughness = map_roughness(roughness);
    vec3 v = normalize(position);
    vec3 r = normalize(reflect(v, normal));

    vec3 world_normal = mat3(u_camera_transform) * normal;
    vec3 world_reflect = mat3(u_camera_transform) * r;
    vec3 diffIr = sample_diffuse(u_sampler_cube[ENV_DIFFUSE], world_normal);

    float spec_mip_lvl = roughness * float(u_num_mip_levels - 1);

    // vec3 specIr = sample_reflection(u_sampler_cube[ENV_SPEC], world_reflect, roughness);
    vec3 specIr = textureLod(u_sampler_cube[ENV_SPEC], world_reflect, spec_mip_lvl).rgb;
    float NoV = clamp(dot(normal, v), 0.0, 1.0);

    vec2 brdfTerm = texture(u_sampler_2D[BRDF_LUT], vec2(NoV, roughness)).rg;

    const vec3 dielectricF0 = vec3(0.04);
    vec3 diffColor = albedo * (1.0 - metalness);// if it is metal, no diffuse color
    vec3 specColor = mix(dielectricF0, albedo, metalness);// since metal has no albedo, we use the space to store its F0

    vec3 distEnvLighting = diffColor * diffIr + specIr * (specColor * brdfTerm.x + brdfTerm.y);
    distEnvLighting *= u_env_light_strength; // * aoVal;
    return distEnvLighting;
}

layout(location = 0) in VertexData
{
    vec4 color;
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 tex_coord = gl_FragCoord.xy / textureSize(u_sampler_2D[ALBEDO], 0);
    vec4 color = texture(u_sampler_2D[ALBEDO], tex_coord);
    vec3 normal = normalize(texture(u_sampler_2D[NORMAL], tex_coord).xyz);
    vec3 position = texture(u_sampler_2D[POSITION], tex_coord).xyz;
    vec3 ao_rough_metal = texture(u_sampler_2D[AO_ROUGH_METAL], tex_coord).rgb;
    out_color = vec4(compute_enviroment_lighting(position, normal, color.rgb, ao_rough_metal.g, ao_rough_metal.b, ao_rough_metal.r), 1.0);
//    out_color = color;
}
