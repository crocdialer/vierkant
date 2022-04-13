#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../renderer/types.glsl"
#include "../renderer/lights_punctual.glsl"

#define ONE_OVER_PI 0.31830988618379067153776752674503

layout(push_constant) uniform PushConstants {
    render_context_t context;
};

layout(std140, binding = 0) uniform ubo_t
{
    mat4 camera_transform;
    mat4 inverse_projection;
    uint num_mip_levels;
    float environment_factor;
    uint num_lights;
} ubo;

#define ALBEDO 0
#define NORMAL 1
#define EMISSION 2
#define AO_ROUGH_METAL 3
#define MOTION 4
#define DEPTH 5
#define BRDF_LUT 6

layout(binding = 1) uniform sampler2D u_sampler_2D[7];

#define ENV_DIFFUSE 0
#define ENV_SPEC 1
layout(binding = 2) uniform samplerCube u_sampler_cube[2];

layout(binding = 3) readonly buffer LightsBuffer{ lightsource_t lights[]; };

vec3 sample_diffuse(in samplerCube diff_map, in vec3 normal)
{
    return texture(diff_map, normal).rgb * ONE_OVER_PI;
}

vec3 compute_enviroment_lighting(vec3 V, vec3 N, vec3 albedo, float roughness, float metalness,
                                 float ambient_occlusion)
{
    vec3 irradiance = sample_diffuse(u_sampler_cube[ENV_DIFFUSE], N);

    vec3 R = normalize(reflect(V, N));
    float spec_mip_lvl = roughness * float(ubo.num_mip_levels - 1);
    vec3 reflection = textureLod(u_sampler_cube[ENV_SPEC], R, spec_mip_lvl).rgb;

    float NoV = clamp(dot(N, V), 0.0, 1.0);
    vec2 brdf = texture(u_sampler_2D[BRDF_LUT], vec2(NoV, roughness)).rg;

    const vec3 dielectricF0 = vec3(0.04);
    vec3 F0 = mix(dielectricF0, albedo, metalness);// since metal has no albedo, we use the space to store its F0
    vec3 F = F_SchlickR(max(NoV, 0.0), F0, roughness);

    // specular reflectance
    vec3 specular = reflection * (F * brdf.x + brdf.y);

    // diffuse based on irradiance
    vec3 diffuse = irradiance * albedo;
    diffuse *= 1.0 - metalness;

    vec3 ambient = diffuse + F0 * specular * ambient_occlusion;

    return ambient;
}

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    float depth = texture(u_sampler_2D[DEPTH], vertex_in.tex_coord).x;

    // reconstruct position from depth
    vec3 clip_pos = vec3(gl_FragCoord.xy / context.size, depth);
    vec4 viewspace_pos = ubo.inverse_projection * vec4(2.0 * clip_pos.xy - 1, clip_pos.z, 1);
    vec3 position = (ubo.camera_transform * (viewspace_pos / viewspace_pos.w)).xyz;

    // view
    vec3 V = normalize(position - ubo.camera_transform[3].xyz);

    // sample g-buffer
    vec4 albedo = texture(u_sampler_2D[ALBEDO], vertex_in.tex_coord);
    vec3 normal = normalize(texture(u_sampler_2D[NORMAL], vertex_in.tex_coord).xyz);
    vec3 ao_rough_metal = texture(u_sampler_2D[AO_ROUGH_METAL], vertex_in.tex_coord).rgb;
    vec3 emission = texture(u_sampler_2D[EMISSION], vertex_in.tex_coord).rgb;

    vec3 env_color = compute_enviroment_lighting(V, normal, albedo.rgb, ao_rough_metal.g, ao_rough_metal.b, ao_rough_metal.r);

    vec3 punctial_light_color = vec3(0);
//    lightsource_t l;
//    l.type = LIGHT_TYPE_DIRECTIONAL;
//    l.position = vec3(0);
//    l.direction = vec3(0, 0, -1.0);
//    l.color = vec3(1);
//    l.intensity = 2.0;
//    l.range = 10000.0;

    for(uint i = 0; i < ubo.num_lights; ++i)
    {
        punctial_light_color += shade(lights[i], V, normal, position, albedo,
                                      ao_rough_metal.g, ao_rough_metal.b, 1.0);
    }
    out_color = vec4(punctial_light_color + env_color * ubo.environment_factor + emission, 1.0);
}
