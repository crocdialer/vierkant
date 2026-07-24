module visibility_test;

import ray_common;
import types;
import "../utils/packed_vertex.slang";
import "../utils/random.slang";

namespace ray
{

//! group information for a ray-hit
public struct RayHit
{
    public uint entry_index;
    public uint primitive_index;
    public float2 barycentrics;
    public float hit_t;
    public bool front_face;
    public bool valid;
};

public RayHit ray_cast(Ray ray, RaytracingAccelerationStructure as)
{
    RayHit ret;
    ret.valid = false;

    RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(as, RAY_FLAG_FORCE_OPAQUE, 0xFF, RayDesc(ray.origin, ray.tmin, ray.direction, ray.tmax));
    query.Proceed();

    if(query.CommittedStatus() != COMMITTED_NOTHING)
    {
        ret.entry_index = query.CommittedInstanceID();
        ret.primitive_index = query.CommittedPrimitiveIndex();
        ret.barycentrics = query.CommittedTriangleBarycentrics();
        ret.hit_t = query.CommittedRayT();
        ret.front_face = query.CommittedTriangleFrontFace();
        ret.valid = true;
    }
    return ret;
}

//! binary shadow-ray visibility. non-opaque candidates get the same alpha-cutout/blend treatment as any_hit(),
//! so alpha-masked geometry no longer casts solid shadows
public bool visibility_test(Ray ray, RaytracingAccelerationStructure as, uint extra_ray_flags,
                            trace_data_t trace_data, Sampler2D textures[], inout uint rng_state)
{
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
    query.TraceRayInline(as, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | extra_ray_flags, 0xFF,
                         RayDesc(ray.origin, ray.tmin, ray.direction, ray.tmax));

    while(query.Proceed())
    {
        if(query.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE) { continue; }

        let entry = trace_data.entries + query.CandidateInstanceIndex();
        material_t material = trace_data.materials[entry->material_index];

        bool blocks = true;
        if(!material.null_surface &&
           (material.blend_mode == BlendMode::Mask || material.blend_mode == BlendMode::Blend))
        {
            if((material.texture_type_flags & uint(TextureTypeBits::Color)) != 0)
            {
                uint base = entry->base_index + 3 * query.CandidatePrimitiveIndex();
                int3 ind = int3(trace_data.index_buffers[entry->buffer_index][base + 0],
                                trace_data.index_buffers[entry->buffer_index][base + 1],
                                trace_data.index_buffers[entry->buffer_index][base + 2]);

                float2 bary = query.CandidateTriangleBarycentrics();
                float3 barycentric = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
                float2 tex_coord =
                        utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.x])
                                        .tex_coord * barycentric.x +
                        utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.y])
                                        .tex_coord * barycentric.y +
                        utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.z])
                                        .tex_coord * barycentric.z;
                material.color *= textures[NonUniformResourceIndex(material.albedo_index)].SampleLevel(tex_coord, 0);
            }
            if(material.blend_mode == BlendMode::Mask && material.color.a < material.alpha_cutoff) { blocks = false; }
            if(material.blend_mode == BlendMode::Blend && material.color.a < utils::rnd(rng_state)) { blocks = false; }
        }
        if(blocks) { query.CommitNonOpaqueTriangleHit(); }
    }
    return query.CommittedStatus() == COMMITTED_NOTHING;
}

//! straight-line transmittance shadow ray: like visibility_test(), but transmissive geometry attenuates instead of
//! blocking. returns the accumulated colour-transmittance (float3(1) = fully visible, float3(0) = fully occluded).
//! per transmissive hit multiplies by `transmission * color.rgb` (straight-line approximation, ignores refraction);
//! null-surface boundaries pass through unattenuated. opaque and alpha-cutout/blend hits still fully block.
//! rng is only drawn for Blend alpha-cutout (as in visibility_test), so transmission-free scenes keep the same budget.
public float3 transmittance_test(Ray ray, RaytracingAccelerationStructure as, uint extra_ray_flags,
                                 trace_data_t trace_data, Sampler2D textures[], inout uint rng_state)
{
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
    query.TraceRayInline(as, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | extra_ray_flags, 0xFF,
                         RayDesc(ray.origin, ray.tmin, ray.direction, ray.tmax));

    float3 transmittance = float3(1);

    while(query.Proceed())
    {
        if(query.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE) { continue; }

        let entry = trace_data.entries + query.CandidateInstanceIndex();
        material_t material = trace_data.materials[entry->material_index];

        // null-surface boundaries do not attenuate shadow rays
        if(material.null_surface) { continue; }

        // transmissive geometry: attenuate and pass through (no rng draw)
        if(material.transmission > 0.0)
        {
            transmittance *= material.transmission * material.color.rgb;
            continue;
        }

        // otherwise mirror visibility_test()'s alpha-cutout / blend blocking
        bool blocks = true;
        if(material.blend_mode == BlendMode::Mask || material.blend_mode == BlendMode::Blend)
        {
            if((material.texture_type_flags & uint(TextureTypeBits::Color)) != 0)
            {
                uint base = entry->base_index + 3 * query.CandidatePrimitiveIndex();
                int3 ind = int3(trace_data.index_buffers[entry->buffer_index][base + 0],
                                trace_data.index_buffers[entry->buffer_index][base + 1],
                                trace_data.index_buffers[entry->buffer_index][base + 2]);

                float2 bary = query.CandidateTriangleBarycentrics();
                float3 barycentric = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
                float2 tex_coord =
                        utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.x])
                                        .tex_coord * barycentric.x +
                        utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.y])
                                        .tex_coord * barycentric.y +
                        utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.z])
                                        .tex_coord * barycentric.z;
                material.color *= textures[NonUniformResourceIndex(material.albedo_index)].SampleLevel(tex_coord, 0);
            }
            if(material.blend_mode == BlendMode::Mask && material.color.a < material.alpha_cutoff) { blocks = false; }
            if(material.blend_mode == BlendMode::Blend && material.color.a < utils::rnd(rng_state)) { blocks = false; }
        }
        if(blocks) { query.CommitNonOpaqueTriangleHit(); }
    }
    // any committed hit (opaque geometry or an alpha-blocking candidate) fully occludes
    return query.CommittedStatus() == COMMITTED_NOTHING ? transmittance : float3(0);
}

}// namespace ray
