#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

//#include "bsdf_UE4.glsl"
#include "bsdf_disney.glsl"

layout(push_constant) uniform PushConstants
{
    push_constants_t push_constants;
};

// array of vertex-buffers
layout(binding = 3, set = 0, scalar) readonly buffer Vertices { Vertex v[]; } vertices[];

// array of index-buffers
layout(binding = 4, set = 0) readonly buffer Indices { uint i[]; } indices[];

layout(binding = 5, set = 0) readonly buffer Entries { entry_t entries[]; };

layout(binding = 6, set = 0) readonly buffer Materials{ material_t materials[]; };

layout(binding = 7) uniform sampler2D u_albedos[];

layout(binding = 8) uniform sampler2D u_normalmaps[];

layout(binding = 9) uniform sampler2D u_emissionmaps[];

layout(binding = 10) uniform sampler2D u_ao_rough_metal_maps[];

// the ray-payload written here
layout(location = 0) rayPayloadInEXT payload_t payload;

// builtin barycentric coords
hitAttributeEXT vec2 attribs;

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
    ivec3 ind = ivec3(indices[nonuniformEXT(entry.buffer_index)].i[entry.base_index + 3 * gl_PrimitiveID + 0],
    indices[nonuniformEXT(entry.buffer_index)].i[entry.base_index + 3 * gl_PrimitiveID + 1],
    indices[nonuniformEXT(entry.buffer_index)].i[entry.base_index + 3 * gl_PrimitiveID + 2]);

    // triangle vertices
    return Triangle(vertices[nonuniformEXT(entry.buffer_index)].v[entry.vertex_offset + ind.x],
                    vertices[nonuniformEXT(entry.buffer_index)].v[entry.vertex_offset + ind.y],
                    vertices[nonuniformEXT(entry.buffer_index)].v[entry.vertex_offset + ind.z]);
}

//! returns a base LoD for a triangle. derives the result by comparing tex-coord and world-space sizes.
float lod_constant(Triangle t)
{
    // transform vertices
    entry_t entry = entries[gl_InstanceCustomIndexEXT];
    t.v0.position = (entry.modelview * vec4(t.v0.position, 1.0)).xyz;
    t.v1.position = (entry.modelview * vec4(t.v1.position, 1.0)).xyz;
    t.v2.position = (entry.modelview * vec4(t.v2.position, 1.0)).xyz;

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

Vertex interpolate_vertex(Triangle t)
{
    const vec3 barycentric = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    // interpolated vertex
    Vertex out_vert;
    out_vert.position = t.v0.position * barycentric.x + t.v1.position * barycentric.y + t.v2.position * barycentric.z;
    out_vert.tex_coord = t.v0.tex_coord * barycentric.x + t.v1.tex_coord * barycentric.y + t.v2.tex_coord * barycentric.z;
    out_vert.normal = t.v0.normal * barycentric.x + t.v1.normal * barycentric.y + t.v2.normal * barycentric.z;
    out_vert.tangent = t.v0.tangent * barycentric.x + t.v1.tangent * barycentric.y + t.v2.tangent * barycentric.z;

    // bring surfel into worldspace
    entry_t entry = entries[gl_InstanceCustomIndexEXT];
    out_vert.position = (entry.modelview * vec4(out_vert.position, 1.0)).xyz;
    out_vert.normal = normalize((entry.normal_matrix * vec4(out_vert.normal, 1.0)).xyz);
    out_vert.tangent = normalize((entry.normal_matrix * vec4(out_vert.tangent, 1.0)).xyz);

    return out_vert;
}

void main()
{
    //    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    Triangle triangle = get_triangle();
    Vertex v = interpolate_vertex(triangle);
    float triangle_lod = lod_constant(triangle);

    material_t material = materials[entries[gl_InstanceCustomIndexEXT].material_index];

    vec3 V = -gl_WorldRayDirectionEXT;
    float NoV = abs(dot(V, v.normal));

    payload.position = v.position;
    payload.normal = v.normal;

    // next ray from current position
    payload.ray.origin = payload.position;

    // propagate ray-cone
    payload.cone = propagate(payload.cone, 0.0, gl_HitTEXT);

    if((material.texture_type_flags & TEXTURE_TYPE_NORMAL) != 0)
    {
        // normalize after checking for validity
        v.tangent = normalize(v.tangent);

        // sample normalmap
        vec3 normal = normalize(2.0 * (sample_texture_lod(u_normalmaps[material.normalmap_index],
        v.tex_coord, NoV, payload.cone.width, triangle_lod).xyz - vec3(0.5)));

        // normal, tangent, bi-tangent
        vec3 b = normalize(cross(v.normal, v.tangent));
        payload.normal = mat3(v.tangent, b, v.normal) * normal;
    }

    // flip the normal so it points against the ray direction:
    payload.ff_normal = faceforward(payload.normal, gl_WorldRayDirectionEXT, v.normal);

    // max emission from material/map
    if((material.texture_type_flags & TEXTURE_TYPE_EMISSION) != 0)
    {
        material.emission.rgb = max(material.emission.rgb, sample_texture_lod(u_emissionmaps[material.emission_index],
                                                                              v.tex_coord, NoV, payload.cone.width,
                                                                              triangle_lod).rgb);

    }
    material.emission.rgb *= dot(payload.normal, payload.ff_normal) > 0 ? material.emission.a : 0.0;

    // absorption in media
    payload.beta *= exp(-payload.absorption * gl_HitTEXT);
    payload.absorption = vec3(0);

    // add radiance from emission
    payload.radiance += payload.beta * material.emission.rgb;

    // albedo
    material.color = push_constants.disable_material ? vec4(vec3(.8), 1.0) : material.color;

    if((material.texture_type_flags & TEXTURE_TYPE_COLOR) != 0)
    {
        material.color *= sample_texture_lod(u_albedos[material.texture_index],
                                             v.tex_coord, NoV, payload.cone.width, triangle_lod);
    }

    // alpha-cutoff
    if(material.blend_mode == BLEND_MODE_MASK && material.color.a < material.alpha_cutoff){ return; }

    // roughness / metalness
    if((material.texture_type_flags & TEXTURE_TYPE_AO_ROUGH_METAL) != 0)
    {
        vec2 rough_metal_tex = sample_texture_lod(u_ao_rough_metal_maps[material.ao_rough_metal_index],
        v.tex_coord, NoV, payload.cone.width, triangle_lod).gb;
        material.roughness *= rough_metal_tex.x;
        material.metalness *= rough_metal_tex.y;
    }

    uint rngState = tea(push_constants.random_seed, gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y + gl_LaunchIDEXT.x);

    float eta = payload.inside_media ? material.ior / payload.ior : payload.ior / material.ior;
    eta += EPS;

    payload.ior = payload.inside_media ? material.ior : 1.0;

    // TODO: compile-time toggle BSDF
//    bsdf_sample_t bsdf_sample = sample_UE4(material, payload.ff_normal, V, eta, rngState);
    bsdf_sample_t bsdf_sample = sample_disney(material, payload.ff_normal, V, eta, rngState);

    payload.ray.direction = bsdf_sample.direction;

    float cos_theta = abs(dot(payload.normal, payload.ray.direction));

    if (bsdf_sample.pdf <= 0.0 || all(lessThan(payload.beta, vec3(0.01))))
    {
        payload.stop = true;
        return;
    }

    payload.beta *= bsdf_sample.F * cos_theta / (bsdf_sample.pdf + EPS);
    payload.beta = clamp(payload.beta, 0.0, 2 * PI);
    payload.inside_media = bsdf_sample.transmission ? !payload.inside_media : payload.inside_media;

    payload.absorption = payload.inside_media ?
                         -log(material.attenuation_color.rgb) / (material.attenuation_distance + EPS) : vec3(0);
}
