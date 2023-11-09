#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_GOOGLE_include_directive : enable

#include "../utils/packed_vertex.glsl"
#include "../utils/phase_function.glsl"
#include "reservoir.glsl"
#include "ray_common.glsl"
#include "bsdf_disney.glsl"
#include "direct_lighting.glsl"

#define USE_DIRECT_LIGHTING 0

//! Triangle groups triangle vertices
struct Triangle
{
    Vertex v0, v1, v2;
};

layout(push_constant) uniform PushConstants
{
    push_constants_t push_constants;
};

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

// array of vertex-buffers
layout(binding = 3, set = 0, scalar) readonly buffer Vertices { packed_vertex_t v[]; } vertices[];

// array of index-buffers
layout(binding = 4, set = 0) readonly buffer Indices { uint i[]; } indices[];

layout(binding = 5, set = 0) readonly buffer Entries { entry_t entries[]; };

layout(binding = 6, set = 0) readonly buffer Materials{ material_t materials[]; };

layout(binding = 7) uniform sampler2D u_textures[];

// the ray-payload written here
layout(location = MISS_INDEX_DEFAULT) rayPayloadInEXT payload_t payload;

// builtin barycentric coords
hitAttributeEXT vec2 attribs;

float max3(vec3 v){ return v.x > v.y ? max(v.x, v.z) : max(v.y, v.z); }

RayCone propagate(RayCone cone, float surface_spread_angle, float hitT)
{
    RayCone new_cone;

    // grow footprint
    new_cone.width = cone.width + cone.spread_angle * hitT;

    // alter spread_angle
    new_cone.spread_angle = cone.spread_angle + surface_spread_angle;
    return new_cone;
}

//! returns the current triangle in object-space
Triangle get_triangle()
{
    // entry aka instance
    entry_t entry = entries[gl_InstanceCustomIndexEXT];

    // triangle indices
    ivec3 ind = ivec3(indices[entry.buffer_index].i[entry.base_index + 3 * gl_PrimitiveID + 0],
                      indices[entry.buffer_index].i[entry.base_index + 3 * gl_PrimitiveID + 1],
                      indices[entry.buffer_index].i[entry.base_index + 3 * gl_PrimitiveID + 2]);

    // triangle vertices
    return Triangle(unpack(vertices[entry.buffer_index].v[entry.vertex_offset + ind.x]),
                    unpack(vertices[entry.buffer_index].v[entry.vertex_offset + ind.y]),
                    unpack(vertices[entry.buffer_index].v[entry.vertex_offset + ind.z]));
}

//! calculates a triangle's normal
vec3 triangle_normal(Triangle t)
{
    return normalize(cross(t.v1.position - t.v0.position, t.v2.position - t.v0.position));
}

//! returns a base LoD for a triangle. derives the result by comparing tex-coord and world-space sizes.
float lod_constant(Triangle t)
{
    // transform vertices
    entry_t entry = entries[gl_InstanceCustomIndexEXT];
    t.v0.position = apply_transform(entry.transform, t.v0.position);
    t.v1.position = apply_transform(entry.transform, t.v1.position);
    t.v2.position = apply_transform(entry.transform, t.v2.position);

    float p_a = length(cross(t.v1.position - t.v0.position, t.v2.position - t.v0.position));
    float t_a = abs((t.v1.tex_coord.x - t.v0.tex_coord.x) * (t.v2.tex_coord.y - t.v0.tex_coord.y) -
                    (t.v2.tex_coord.x - t.v0.tex_coord.x) * (t.v1.tex_coord.y - t.v0.tex_coord.y));
    return 0.5 * log2(t_a / p_a);
}

vec4 sample_texture_lod(sampler2D tex, vec2 tex_coord, float NoV, float cone_width, float lambda)
{
    vec2 sz = textureSize(tex, 0);

    // Eq . 34
    lambda += log2(abs(cone_width));
    lambda += 0.5 * log2(sz.x * sz.y);
    lambda -= log2(NoV);
    return textureLod(tex, tex_coord, lambda);
}

float channel_avg(vec3 v)
{
    return (v.x + v.y + v.z) / 3.0;
}

Vertex interpolate_vertex(Triangle t)
{
    const vec3 barycentric = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    // interpolated vertex
    Vertex out_vert;
    out_vert.position = t.v0.position * barycentric.x + t.v1.position * barycentric.y + t.v2.position * barycentric.z;
    out_vert.tex_coord = t.v0.tex_coord * barycentric.x + t.v1.tex_coord * barycentric.y + t.v2.tex_coord * barycentric.z;
    out_vert.normal = normalize(t.v0.normal * barycentric.x + t.v1.normal * barycentric.y + t.v2.normal * barycentric.z);
    out_vert.tangent = normalize(t.v0.tangent * barycentric.x + t.v1.tangent * barycentric.y + t.v2.tangent * barycentric.z);

    // bring surfel into worldspace
    entry_t entry = entries[gl_InstanceCustomIndexEXT];
    out_vert.position = apply_transform(entry.transform, out_vert.position);
    out_vert.tex_coord = (entry.texture_matrix * vec4(out_vert.tex_coord, 0.f, 1.0)).xy;

    vec4 quat = vec4(entry.transform.rotation_x, entry.transform.rotation_y, entry.transform.rotation_z,
                     entry.transform.rotation_w);
    out_vert.normal = rotate_quat(quat, out_vert.normal);
    out_vert.tangent = rotate_quat(quat, out_vert.tangent);

    // account for two-sided materials seen from backside, flip normals
    if(gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT){ out_vert.normal *= -1.0; }
    return out_vert;
}

void main()
{
    uint rng_state = payload.rng_state;
    Triangle triangle = get_triangle();
    Vertex v = interpolate_vertex(triangle);
    float triangle_lod = lod_constant(triangle);

    material_t material = materials[entries[gl_InstanceCustomIndexEXT].material_index];

    vec3 V = -gl_WorldRayDirectionEXT;
    float NoV = abs(dot(V, payload.normal));

    payload.position = v.position;
    payload.normal = v.normal;

    // next ray from current position
    payload.ray.origin = payload.position;

    // participating media
    bool sample_medium = false;

    if(any(greaterThan(payload.sigma_t, vec3(0))))
    {
        // sample a ray hit_t
        int channel = int(min(rnd(rng_state) * 3, 2));
        float t = min(-log(1 - rnd(rng_state)) / payload.sigma_t[channel], gl_HitTEXT);

        // fraction of scattering vs. absorption
        vec3 sigma_s = material.scattering_ratio * payload.sigma_t;

        // determine scattering
        sample_medium = t < gl_HitTEXT;

        // beam_transmittance
        vec3 beam_tr = exp(-payload.sigma_t * t);
        vec3 density = sample_medium ? beam_tr * payload.sigma_t : beam_tr;
        float pdf = channel_avg(density);
        payload.beta *= sample_medium ? (sigma_s * beam_tr / pdf) : (beam_tr / pdf);

        // sample scattering event
        if(sample_medium)
        {
            const float g = material.phase_asymmetry_g;
            vec2 Xi = vec2(rnd(rng_state), rnd(rng_state));
            payload.ray.origin += payload.ray.direction * t;
            float phase_pdf;
            payload.ray.direction = local_frame(-payload.ray.direction) * sample_phase_hg(Xi, g, phase_pdf);

            #if USE_DIRECT_LIGHTING
            sunlight_params_t sun_params;
            sun_params.color = vec3(1.0, 0.6, 0.4);
            sun_params.intensity = 1.0;
            sun_params.direction = normalize(vec3(.4, 1.0, 0.7));
            sun_params.angular_size = 0.524167 *  PI / 180.0;
            vec3 sun_L = phase_pdf * sample_sun_light_phase(payload.ray, sun_params, topLevelAS, rng_state);
            payload.radiance += payload.beta * sun_L;
            #endif
        }
    }

    // albedo
    if((material.texture_type_flags & TEXTURE_TYPE_COLOR) != 0)
    {
        material.color *= sample_texture_lod(u_textures[material.albedo_index],
                                             v.tex_coord, NoV, payload.cone.width, triangle_lod);
    }
    material.color = push_constants.disable_material ? vec4(vec3(.8), 1.0) : material.color;

    // skip surface-interaction (alpha-cutoff/blend)
    if(material.blend_mode == BLEND_MODE_MASK && material.color.a < material.alpha_cutoff ||
       material.blend_mode == BLEND_MODE_BLEND && material.color.a < rnd(rng_state))
    {
        material.null_surface = true;
    }

    if((material.texture_type_flags & TEXTURE_TYPE_NORMAL) != 0)
    {
        // normalize after checking for validity
        v.tangent = normalize(v.tangent);

        // sample normalmap
        vec3 normal = normalize(2.0 * (sample_texture_lod(u_textures[material.normalmap_index],
                                                          v.tex_coord, NoV, payload.cone.width, triangle_lod).xyz -
                                       vec3(0.5)));

        // normal, tangent, bi-tangent
        vec3 b = normalize(cross(v.normal, v.tangent));
        payload.normal = mat3(v.tangent, b, payload.normal) * normal;
    }

    // flip the normal so it points against the ray direction:
    payload.ff_normal = faceforward(payload.normal, gl_WorldRayDirectionEXT, payload.normal);

    // max emission from material/map
    if((material.texture_type_flags & TEXTURE_TYPE_EMISSION) != 0)
    {
        material.emission.rgb = max(material.emission.rgb, sample_texture_lod(u_textures[material.emission_index],
                                                                              v.tex_coord, NoV, payload.cone.width,
                                                                              triangle_lod).rgb);

    }
    material.emission.rgb *= dot(payload.normal, payload.ff_normal) > 0 ? material.emission.a : 0.0;

    // add radiance from emission
    payload.radiance += payload.beta * material.emission.rgb;

    // roughness / metalness
    if((material.texture_type_flags & TEXTURE_TYPE_AO_ROUGH_METAL) != 0)
    {
        vec2 rough_metal_tex = sample_texture_lod(u_textures[material.ao_rough_metal_index],
                                                  v.tex_coord, NoV, payload.cone.width, triangle_lod).gb;
        material.roughness *= rough_metal_tex.x;
        material.metalness *= rough_metal_tex.y;
    }

    float eta = payload.transmission ? material.ior / payload.ior : payload.ior / material.ior;
    eta += EPS;

    payload.ior = payload.transmission ? material.ior : 1.0;
    bool sample_surface = !(material.null_surface || sample_medium);

    if(sample_surface)
    {
        #if USE_DIRECT_LIGHTING
        sunlight_params_t sun_params;
        sun_params.color = vec3(1.0, 0.6, 0.4);
        sun_params.intensity = 1.0;
        sun_params.direction = normalize(vec3(.4, 1.0, 0.7));
        sun_params.angular_size = 0.524167 *  PI / 180.0;
        vec3 sun_L = sample_sun_light(material, sun_params, topLevelAS, payload.position, payload.ff_normal, V, eta,
        rng_state);
        payload.radiance += payload.beta * sun_L;
        #endif

        // propagate ray-cone
        payload.cone = propagate(payload.cone, 0.0, gl_HitTEXT);

        // take sample from burley/disney BSDF
        bsdf_sample_t bsdf_sample = sample_disney(material, payload.ff_normal, V, eta, rng_state);

        // TODO: check wtf/when pdf turns out NaN here
        if(bsdf_sample.pdf <= 0.0 || isnan(bsdf_sample.pdf)){ payload.stop = true; return; }

        payload.ray.direction = bsdf_sample.direction;
        float cos_theta = abs(dot(payload.normal, bsdf_sample.direction));

        payload.beta *= bsdf_sample.F * cos_theta / (bsdf_sample.pdf + PDF_EPS);
        payload.transmission = bsdf_sample.transmission ? !payload.transmission : payload.transmission;

        // TODO: probably better to offset origin after bounces, instead of biasing ray-tmin!?
//        payload.ray.origin += (bsdf_sample.transmission ? -1.0 : 1.0) * payload.ff_normal * EPS;
    }
    else
    {
        payload.transmission = payload.transmission ^^ (material.transmission > 0.0);
    }

    vec3 sigma_t = -log(material.attenuation_color.rgb) / material.attenuation_distance;
    payload.sigma_t = payload.transmission ? sigma_t : vec3(0);

    // Russian roulette
    if(max3(payload.beta) <= 0.05 && payload.depth >= 2)
    {
        float q = 1.0 - max3(payload.beta);

        if(rnd(rng_state) < q)
        {
            payload.stop = true;
            return;
        }
        payload.beta /= (1.0 - q);
    }
}
