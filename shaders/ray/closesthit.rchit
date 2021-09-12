#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

const uint MAX_NUM_ENTRIES = 1024;

layout(push_constant) uniform PushConstants
{
    push_constants_t push_constants;
};

// array of vertex-buffers
layout(binding = 3, set = 0, scalar) readonly buffer Vertices { Vertex v[]; } vertices[];

// array of index-buffers
layout(binding = 4, set = 0) readonly buffer Indices { uint i[]; } indices[];

layout(binding = 5, set = 0) uniform Entries
{
    entry_t u_entries[MAX_NUM_ENTRIES];
};

layout(binding = 6, set = 0) uniform Materials
{
    material_t materials[MAX_NUM_ENTRIES];
};

layout(binding = 7) uniform sampler2D u_albedos[];

layout(binding = 8) uniform sampler2D u_normalmaps[];

layout(binding = 9) uniform sampler2D u_emissionmaps[];

layout(binding = 10) uniform sampler2D u_ao_rough_metal_maps[];

// the ray-payload written here
layout(location = 0) rayPayloadInEXT payload_t payload;

// builtin barycentric coords
hitAttributeEXT vec2 attribs;

Vertex interpolate_vertex()
{
    const vec3 barycentric = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    // entry aka instance
    entry_t entry = u_entries[gl_InstanceCustomIndexEXT];

    // triangle indices
    ivec3 ind = ivec3(indices[nonuniformEXT(entry.buffer_index)].i[entry.base_index + 3 * gl_PrimitiveID + 0],
    indices[nonuniformEXT(entry.buffer_index)].i[entry.base_index + 3 * gl_PrimitiveID + 1],
    indices[nonuniformEXT(entry.buffer_index)].i[entry.base_index + 3 * gl_PrimitiveID + 2]);

    // triangle vertices
    Vertex v0 = vertices[nonuniformEXT(entry.buffer_index)].v[entry.base_vertex + ind.x];
    Vertex v1 = vertices[nonuniformEXT(entry.buffer_index)].v[entry.base_vertex + ind.y];
    Vertex v2 = vertices[nonuniformEXT(entry.buffer_index)].v[entry.base_vertex + ind.z];

    // interpolated vertex
    Vertex out_vert;
    out_vert.position = v0.position * barycentric.x + v1.position * barycentric.y + v2.position * barycentric.z;
    out_vert.color = v0.color * barycentric.x + v1.color * barycentric.y + v2.color * barycentric.z;
    out_vert.tex_coord = v0.tex_coord * barycentric.x + v1.tex_coord * barycentric.y + v2.tex_coord * barycentric.z;
    out_vert.normal = v0.normal * barycentric.x + v1.normal * barycentric.y + v2.normal * barycentric.z;
    out_vert.tangent = v0.tangent * barycentric.x + v1.tangent * barycentric.y + v2.tangent * barycentric.z;

    // bring surfel into worldspace
    out_vert.position = (entry.modelview * vec4(out_vert.position, 1.0)).xyz;
    out_vert.normal = normalize((entry.normal_matrix * vec4(out_vert.normal, 1.0)).xyz);
    out_vert.tangent = (entry.normal_matrix * vec4(out_vert.tangent, 1.0)).xyz;

    return out_vert;
}

void main()
{
    //    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    Vertex v = interpolate_vertex();

    material_t material = materials[u_entries[gl_InstanceCustomIndexEXT].material_index];

    payload.position = v.position;
    payload.normal = v.normal;

    bool tangent_valid = any(greaterThan(abs(v.tangent), vec3(0.0)));

    // local frame aka tbn-matrix
    mat3 local_frame = local_frame(v.normal);

    if (tangent_valid)
    {
        // normalize after checking for validity
        v.tangent = normalize(v.tangent);

        // sample normalmap
        vec3 normal = normalize(2.0 * (texture(u_normalmaps[material.normalmap_index], v.tex_coord).xyz - vec3(0.5)));

        // normal, tangent, bi-tangent
        vec3 b = normalize(cross(v.normal, v.tangent));
        local_frame = mat3(v.tangent, b, v.normal);
        payload.normal = local_frame * normal;
    }

    // flip the normal so it points against the ray direction:
    payload.normal = faceforward(payload.normal, gl_WorldRayDirectionEXT, payload.normal);

    // max emission from material/map
    const float emission_tex_gain = 10.0;
    vec3 emission = max(material.emission.rgb, emission_tex_gain * texture(u_emissionmaps[material.emission_index], v.tex_coord).rgb);

    // add radiance from emission
    payload.radiance += payload.beta * emission;

//    if(any(greaterThan(emission, vec3(0.01))))
//    {
//        payload.stop = true;
//        return;
//    }

    // modulate beta with albedo
    vec3 color = push_constants.disable_material ?
        vec3(1) : material.color.rgb * texture(u_albedos[material.texture_index], v.tex_coord).rgb;

    // roughness / metalness
    vec2 rough_metal = texture(u_ao_rough_metal_maps[material.ao_rough_metal_index], v.tex_coord).gb;
    float roughness = material.roughness * rough_metal.x;
    float metalness = material.metalness * rough_metal.y;

    // generate a bounce ray

    // offset position along the normal
    payload.ray.origin = payload.position;// + 0.0001 * payload.normal;

    // scatter ray direction
    uint rngState = rng_seed(push_constants.random_seed);
    vec2 Xi = vec2(rng_float(rngState), rng_float(rngState));

    // no diffuse rays for metal
    float diffuse_ratio = 0.5 * (1.0 - metalness);
    float reflect_prob = rng_float(rngState);

    // possible half-vector from GGX distribution
    vec3 H = local_frame * sample_GGX(Xi, roughness);
    vec3 V = -gl_WorldRayDirectionEXT;

    const bool hit_front = gl_HitKindEXT == gl_HitKindFrontFacingTriangleEXT;

    if(payload.inside_media || reflect_prob < diffuse_ratio)
    {
        float transmission_prob = hit_front ? rng_float(rngState) : 0.0;

        if(transmission_prob < material.transmission)
        {
            float ior = hit_front ? material.ior : 1.0;

            // volume attenuation
            payload.beta *= mix(vec3(1), payload.attenuation, clamp(gl_HitTEXT / payload.attenuation_distance, 0, 1));

            payload.attenuation = hit_front ? material.attenuation_color.rgb : vec3(1);
            payload.attenuation_distance = material.attenuation_distance;
            payload.inside_media = hit_front;

            // transmission/refraction
            float eta = payload.ior / ior;
            payload.ior = ior;
            payload.ray.direction = refract(gl_WorldRayDirectionEXT, H, eta);

            // offset position along the normal
            payload.normal *= -1.0;
            V *= -1.0;

            // offset inside
//            payload.ray.origin = payload.position + 0.0001 * payload.normal;
        }
        else
        {
            // diffuse reflection
            payload.ray.direction = local_frame * sample_cosine(Xi);
        }
    }
    else
    {
        // surface/glossy reflection
        payload.ray.direction = reflect(gl_WorldRayDirectionEXT, H);
    }

    // TODO: decide on recursion here
//    payload.radiance += directLight(material) * payload.beta;

    if(hit_front)
    {
        const float eps =  0.001;
        float pdf = UE4Pdf(payload.ray.direction, payload.normal, V, roughness, metalness);
        float cosTheta = abs(dot(payload.normal, payload.ray.direction));
        vec3 F = UE4Eval(payload.ray.direction, payload.normal, V, color, roughness, metalness);
        payload.beta *= F * cosTheta / (pdf + eps);
        payload.pdf *= pdf;
    }

    // new rays won't contribute much
    if (all(lessThan(payload.beta, vec3(0.01)))){ payload.stop = true; }
}
