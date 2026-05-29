module raypipeline;

import "ray/types";
import "ray/ray_common";
import "ray/pixel_data";
import "ray/reservoir";
import "ray/bsdf_disney";
import "ray/direct_lighting";

import "utils/transform";
import "utils/packed_vertex";
import "utils/random";
import "utils/hash";
import "utils/sampling";
import "utils/constants";
import "utils/simplex";
import "utils/sdf";
import "utils/phase_function";
import "utils/procedural_environment";

static const uint MAX_MEDIA_STACK_SIZE = 4;

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure topLevelAS;

[[vk::binding(1, 0)]]
ConstantBuffer<ray::trace_data_t> trace_data;

[[vk::binding(2, 0)]]
Sampler2D u_textures[];

[[vk::binding(3, 0)]]
SamplerCube u_sampler_cube;

//-----------------------------------------------------------------------------
// ray-generation
//-----------------------------------------------------------------------------

struct trace_result_t
{
    float3 radiance;
    float coverage;
    float3 normal;
    float depth;
    float3 position;
    uint entity_index;
};

ray::Ray camera_ray(float2 coord, float2 size, float2 Xi)
{
    float2 inUV = coord / size;
    float2 d = inUV * 2.0 - 1.0;
    float3 origin, direction;

    if(trace_data.camera.ortho)
    {
        origin = mul(trace_data.camera.view_inverse, mul(trace_data.camera.projection_inverse, float4(d.x, d.y, 1, 1)))
                         .xyz;
        direction = -float3(trace_data.camera.view_inverse[0][2], trace_data.camera.view_inverse[1][2],
                            trace_data.camera.view_inverse[2][2]);
    }
    else
    {
        float3 lens_offset = float3(trace_data.camera.aperture / 2.0 * utils::sample_unit_disc(Xi), 0.0);
        origin = mul(trace_data.camera.view_inverse, float4(lens_offset, 1)).xyz;

        float3 target_dir = normalize(mul(trace_data.camera.projection_inverse, float4(d.x, d.y, 1, 1)).xyz);
        float3 fpoint = target_dir * trace_data.camera.focal_distance / abs(target_dir.z);
        float3x3 view_rot = float3x3(trace_data.camera.view_inverse[0].xyz, trace_data.camera.view_inverse[1].xyz,
                                     trace_data.camera.view_inverse[2].xyz);
        direction = normalize(mul(view_rot, fpoint - lens_offset));
    }

    ray::Ray r;
    r.origin = origin;
    r.direction = direction;
    r.tmin = ray::EPS;
    r.tmax = ray::DEFAULT_TMAX;
    return r;
}

trace_result_t trace(uint2 coord, uint2 size)
{
    trace_result_t result;

    uint rng_state = utils::xxhash32(trace_data.params.random_seed, size.x * coord.y + coord.x);

    result.radiance = float3(0);
    result.coverage = 0.0;

    float sample_offset = 2.0 * utils::rnd(rng_state) - 1.0;

    float spread_angle = atan(2.0 * tan(trace_data.camera.fov / 2.0) / float(size.y));

    if(trace_data.params.restir_gi != 0)
    {
        // -----------------------------------------------------------------
        // Phase 1: reservoir-based importance sampling at the first indirect
        // bounce.  One primary ray establishes the visible point; then
        // restir_candidates sub-paths from x_v build a reservoir.  The
        // selected sample shades the pixel; the reservoir is written for
        // Phase 2 temporal reuse.
        // -----------------------------------------------------------------

        // --- Primary ray (depth 0) — fixed sub-pixel offset so all candidates
        //     share the same visible-point surface.
        float2 rnd_coord = float2(coord) +
                           frac(utils::Hammersley(0, trace_data.params.num_samples) + float2(sample_offset));
        float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
        ray::Ray vis_ray = camera_ray(rnd_coord, float2(size), Xi);

        ray::RayCone vis_cone;
        vis_cone.spread_angle = spread_angle;
        vis_cone.width = 0.0;

        ray::payload_t vis_p;
        vis_p.rng_state    = utils::xxhash32(rng_state, 0u);
        vis_p.depth        = 0;
        vis_p.stop         = false;
        vis_p.radiance     = float3(0);
        vis_p.beta         = float3(1);
        vis_p.media        = trace_data.camera_media;
        vis_p.last_ior     = trace_data.camera_media.ior;
        vis_p.ray          = vis_ray;
        vis_p.cone         = vis_cone;
        vis_p.entity_index = utils::MAX_UINT16;
        vis_p.media_op     = ray::MediaOp::NoOp;
        {
            RayDesc d;
            d.Origin    = vis_ray.origin;
            d.TMin      = vis_ray.tmin;
            d.Direction = vis_ray.direction;
            d.TMax      = vis_ray.tmax;
            TraceRay(topLevelAS, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xff, 0, 0,
                     ray::MISS_INDEX_DEFAULT, d, vis_p);
        }

        // Populate G-buffer (mirrors the i==0 block in the original path)
        result.entity_index = vis_p.entity_index;
        result.normal       = vis_p.normal;
        result.position     = vis_p.position;
        {
            float4 dp = mul(trace_data.camera.projection_view, float4(result.position, 1.0));
            bool miss = vis_p.entity_index == utils::MAX_UINT16;
            result.depth    = miss ? 0.0 : dp.z / dp.w;
            result.coverage = miss ? 0.0 : 1.0;
        }

        // Sky hit: nothing to resample, return sky radiance directly.
        if(vis_p.entity_index == utils::MAX_UINT16)
        {
            result.radiance = vis_p.radiance;
            return result;
        }

        float3       x_v      = vis_p.position;
        float3       n_v      = vis_p.normal;
        float3       V_v      = -vis_ray.direction;
        float3       depth0_L = vis_p.radiance;   // emission + direct at x_v
        ray::media_t vis_media = vis_p.media;

        // --- Build reservoir by streaming restir_candidates sub-paths ---
        ray::ReservoirGI reservoir;
        reservoir.position   = float3(0);
        reservoir.normal     = float3(0, 1, 0);
        reservoir.radiance   = float3(0);
        reservoir.weight_sum = 0.0;
        reservoir.M         = 0;
        reservoir.age       = 0;

        uint num_cands = max(1u, trace_data.params.restir_candidates);
        for(uint k = 0; k < num_cands; k++)
        {
            // Per-candidate seed (prime offset avoids correlation with depth seeds).
            uint cand_rng = utils::xxhash32(rng_state, k + 7919u);

            ray::bsdf_sample_t bsdf_k = ray::sample_disney(
                vis_p.vis_material, n_v, V_v, vis_p.vis_eta, cand_rng);

            if(bsdf_k.pdf <= ray::PDF_EPS) { continue; }

            float cos_k = abs(dot(n_v, bsdf_k.direction));

            // Sub-path from x_v: beta starts at 1 so Lo is NOT weighted by
            // the x_v BSDF factor (keeps Lo separable for Phase 2 reuse).
            ray::payload_t sub;
            sub.rng_state    = cand_rng;
            sub.depth        = 1;
            sub.stop         = false;
            sub.radiance     = float3(0);
            sub.beta         = float3(1);
            sub.media        = vis_media;
            sub.last_ior     = vis_media.ior;
            sub.ray.origin   = x_v;
            sub.ray.direction = bsdf_k.direction;
            sub.ray.tmin     = ray::EPS;
            sub.ray.tmax     = ray::DEFAULT_TMAX;
            sub.cone         = vis_cone;
            sub.entity_index = utils::MAX_UINT16;
            sub.media_op     = ray::MediaOp::NoOp;

            uint sub_mi = 0;
            ray::media_t sub_stack[MAX_MEDIA_STACK_SIZE];
            sub_stack[sub_mi] = vis_media;

            // x_s default: treat sky as a surface at DEFAULT_TMAX distance.
            float3 xs_k       = x_v + bsdf_k.direction * ray::DEFAULT_TMAX;
            float3 ns_k       = -bsdf_k.direction;
            bool   xs_captured = false;

            for(uint j = 1; j < trace_data.params.max_trace_depth; j++)
            {
                sub.rng_state = utils::xxhash32(cand_rng, j);
                sub.depth     = j;
                sub.stop      = false;
                sub.media     = sub_stack[sub_mi];
                sub.last_ior  = sub_stack[sub_mi > 0u ? sub_mi - 1u : 0u].ior;

                uint sub_flags = sub_mi > 0u ? 0u : RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
                RayDesc sd;
                sd.Origin    = sub.ray.origin;
                sd.TMin      = sub.ray.tmin;
                sd.Direction = sub.ray.direction;
                sd.TMax      = sub.ray.tmax;
                TraceRay(topLevelAS, sub_flags, 0xff, 0, 0, ray::MISS_INDEX_DEFAULT, sd, sub);

                if(sub.media_op == ray::MediaOp::Enter)
                {
                    sub_mi = min(sub_mi + 1u, MAX_MEDIA_STACK_SIZE - 1u);
                    sub_stack[sub_mi] = sub.media;
                }
                else if(sub.media_op == ray::MediaOp::Leave) { sub_mi = sub_mi > 0u ? sub_mi - 1u : 0u; }

                if(!xs_captured && sub.entity_index != utils::MAX_UINT16)
                {
                    xs_k       = sub.position;
                    ns_k       = sub.normal;
                    xs_captured = true;
                }

                if(sub.stop) { break; }
            }

            float3 Lo_k    = sub.radiance;
            float  p_hat_k = ray::target_pdf(bsdf_k.F * cos_k * Lo_k);

            ray::ReservoirGI cand = ray::make_reservoir(xs_k, ns_k, Lo_k, bsdf_k.pdf);
            ray::combine_reservoirs(reservoir, cand, utils::rnd(cand_rng), p_hat_k);
        }

        // --- Final shade ---
        float3 indirect = float3(0);
        if(reservoir.M > 0 && reservoir.weight_sum > 0.0)
        {
            float3 dir_sel = normalize(reservoir.position - x_v);
            float  cos_sel = abs(dot(n_v, dir_sel));
            float  dummy_pdf;
            float3 f_sel     = ray::eval_disney(vis_p.vis_material, dir_sel, n_v, V_v,
                                                vis_p.vis_eta, dummy_pdf);
            float  p_hat_sel = ray::target_pdf(f_sel * cos_sel * reservoir.radiance);
            float  W         = p_hat_sel > 0.0
                               ? reservoir.weight_sum / (float(reservoir.M) * p_hat_sel)
                               : 0.0;
            indirect = f_sel * cos_sel * reservoir.radiance * W;
        }

        // Write reservoir for Phase 2 temporal reuse.
        uint buf_idx = coord.x + coord.y * size.x;
        trace_data.reservoirs_curr[buf_idx] = ray::pack(reservoir);

        result.radiance = depth0_L + indirect;
        return result;
    }

    for(int sampleIdx = 0; sampleIdx < int(trace_data.params.num_samples); sampleIdx++)
    {
        float2 rnd_coord = float2(coord) +
                           frac(utils::Hammersley(sampleIdx, trace_data.params.num_samples) + float2(sample_offset));
        float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));

        ray::Ray next_ray = camera_ray(rnd_coord, float2(size), Xi);

        ray::RayCone cone;
        cone.spread_angle = spread_angle;
        cone.width = 0.0;

        uint media_index = 0;
        ray::media_t media[MAX_MEDIA_STACK_SIZE];
        media[media_index] = trace_data.camera_media;

        float3 beta = float3(1);
        float3 path_radiance = float3(0);

        ray::payload_t payload;

        for(uint i = 0; i < trace_data.params.max_trace_depth; i++)
        {
            rng_state = utils::xxhash32(rng_state, i);

            payload.rng_state = rng_state;
            payload.depth = i;
            payload.stop = false;
            payload.radiance = path_radiance;
            payload.beta = beta;
            payload.media = media[media_index];
            payload.last_ior = media[media_index > 0u ? media_index - 1u : 0u].ior;
            payload.ray = next_ray;
            payload.cone = cone;
            payload.entity_index = utils::MAX_UINT16;

            uint ray_flags = media_index > 0u ? 0u : RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

            RayDesc ray_desc;
            ray_desc.Origin = next_ray.origin;
            ray_desc.TMin = next_ray.tmin;
            ray_desc.Direction = next_ray.direction;
            ray_desc.TMax = next_ray.tmax;

            //let hit = HitObject.TraceRay(topLevelAS, ray_flags, 0xff, 0, 0, ray::MISS_INDEX_DEFAULT, ray_desc, payload);
            //ReorderThread(hit);
            //HitObject.Invoke(topLevelAS, hit, payload);
            TraceRay(topLevelAS, ray_flags, 0xff, 0, 0, ray::MISS_INDEX_DEFAULT, ray_desc, payload);

            cone = payload.cone;
            next_ray = payload.ray;
            beta = payload.beta;

            if(payload.media_op == ray::MediaOp::Enter)
            {
                media_index = min(media_index + 1, MAX_MEDIA_STACK_SIZE - 1);
                media[media_index] = payload.media;
            }
            else if(payload.media_op == ray::MediaOp::Leave) { media_index = media_index > 0u ? media_index - 1u : 0u; }

            path_radiance = payload.radiance;

            if(i == 0)
            {
                result.entity_index = payload.entity_index;
                result.normal = payload.normal;
                result.position = payload.position;
                float4 depth_proj = mul(trace_data.camera.projection_view, float4(result.position, 1.0));
                bool ray_miss = payload.entity_index == utils::MAX_UINT16;
                result.depth = ray_miss ? 0.0 : depth_proj.z / depth_proj.w;
            }

            if(payload.stop) { break; }
        }

        result.radiance += payload.radiance;
        result.coverage = max(result.coverage, payload.depth > 0u ? 1.0 : 0.0);
    }

    result.radiance /= float(trace_data.params.num_samples);
    return result;
}

[shader("raygeneration")]
void main()
{
    uint2 coord = DispatchRaysIndex().xy;
    uint2 size = DispatchRaysDimensions().xy;

    trace_result_t result = trace(coord, size);
    uint buffer_idx = coord.x + coord.y * size.x;

    float4 accumulated_radiance = float4(result.radiance, smoothstep(0.0, 1.0, result.coverage));

    if(trace_data.params.batch_index != 0)
    {
        uint batch_size = trace_data.params.batch_index;
        accumulated_radiance += batch_size * float4(trace_data.out_pixels[buffer_idx].radiance,
                                                    trace_data.out_pixels[buffer_idx].alpha);
        accumulated_radiance /= batch_size + 1;
    }

    trace_data.out_pixels[buffer_idx] =
            ray::pack(accumulated_radiance.rgb, result.coverage, result.normal, result.depth, result.entity_index);
}

//-----------------------------------------------------------------------------
// closest-hit
//-----------------------------------------------------------------------------

static const uint HIT_KIND_BACK_FACE = 0xFF;

float max3(float3 v) { return v.x > v.y ? max(v.x, v.z) : max(v.y, v.z); }

ray::RayCone propagate(ray::RayCone cone, float surface_spread_angle, float hitT)
{
    ray::RayCone new_cone;
    new_cone.width = cone.width + cone.spread_angle * hitT;
    new_cone.spread_angle = cone.spread_angle + surface_spread_angle;
    return new_cone;
}

ray::Triangle get_triangle(Ptr<ray::entry_t, Access::Read> entry)
{
    uint base = entry->base_index + 3 * PrimitiveIndex();
    int3 ind = int3(trace_data.index_buffers[entry->buffer_index][base + 0],
                    trace_data.index_buffers[entry->buffer_index][base + 1],
                    trace_data.index_buffers[entry->buffer_index][base + 2]);

    return ray::Triangle(utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.x]),
                         utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.y]),
                         utils::unpack(trace_data.vertex_buffers[entry->buffer_index][entry->vertex_offset + ind.z]));
}

float lod_constant(Ptr<ray::entry_t, Access::Read> entry, ray::Triangle t)
{
    t.v0.position = utils::apply_transform(entry->transform, t.v0.position);
    t.v1.position = utils::apply_transform(entry->transform, t.v1.position);
    t.v2.position = utils::apply_transform(entry->transform, t.v2.position);

    float p_a = length(cross(t.v1.position - t.v0.position, t.v2.position - t.v0.position));
    float t_a = abs((t.v1.tex_coord.x - t.v0.tex_coord.x) * (t.v2.tex_coord.y - t.v0.tex_coord.y) -
                    (t.v2.tex_coord.x - t.v0.tex_coord.x) * (t.v1.tex_coord.y - t.v0.tex_coord.y));
    return 0.5 * log2(t_a / p_a);
}

float4 sample_texture_lod(Sampler2D tex, float2 tex_coord, float NoV, float cone_width, float lambda)
{
    uint w, h;
    tex.GetDimensions(w, h);
    float2 sz = float2(w, h);
    lambda += log2(abs(cone_width));
    lambda += 0.5 * log2(sz.x * sz.y);
    lambda -= log2(NoV);
    return tex.SampleLevel(tex_coord, lambda);
}

utils::transform_t to_aabb_norm(Ptr<ray::entry_t, Access::Read> entry)
{
    float3 aabb_size = float3(entry->aabb.max_x - entry->aabb.min_x, entry->aabb.max_y - entry->aabb.min_y,
                              entry->aabb.max_z - entry->aabb.min_z);
    utils::transform_t ret = entry->inv_transform;
    ret.scale_x /= aabb_size.x;
    ret.scale_y /= aabb_size.y;
    ret.scale_z /= aabb_size.z;
    return ret;
}

float channel_avg(float3 v) { return (v.x + v.y + v.z) / 3.0; }

utils::Vertex interpolate_vertex(Ptr<ray::entry_t, Access::Read> entry, ray::Triangle t, float2 bary_coords)
{
    float3 barycentric = float3(1.0 - bary_coords.x - bary_coords.y, bary_coords.x, bary_coords.y);

    utils::Vertex out_vert;
    float3 position = t.v0.position * barycentric.x + t.v1.position * barycentric.y + t.v2.position * barycentric.z;
    out_vert.tex_coord =
            t.v0.tex_coord * barycentric.x + t.v1.tex_coord * barycentric.y + t.v2.tex_coord * barycentric.z;
    out_vert.normal =
            normalize(t.v0.normal * barycentric.x + t.v1.normal * barycentric.y + t.v2.normal * barycentric.z);
    out_vert.tangent =
            normalize(t.v0.tangent * barycentric.x + t.v1.tangent * barycentric.y + t.v2.tangent * barycentric.z);

    out_vert.position = utils::apply_transform(entry->transform, position);
    out_vert.tex_coord = mul(entry->texture_matrix, float4(out_vert.tex_coord, 0.0, 1.0)).xy;

    float4 quat = float4(entry->transform.rotation_x, entry->transform.rotation_y, entry->transform.rotation_z,
                         entry->transform.rotation_w);
    out_vert.normal = utils::rotate_quat(quat, out_vert.normal);
    out_vert.normal = (HitKind() == HIT_KIND_BACK_FACE) ? -out_vert.normal : out_vert.normal;
    out_vert.tangent = utils::rotate_quat(quat, out_vert.tangent);
    return out_vert;
}

[shader("closesthit")]
void closest_hit(inout ray::payload_t payload, BuiltInTriangleIntersectionAttributes attribs)
{
    uint rng_state = payload.rng_state;
    let entry = trace_data.entries + InstanceIndex();

    ray::Triangle triangle = get_triangle(entry);
    utils::Vertex v = interpolate_vertex(entry, triangle, attribs.barycentrics);
    float triangle_lod = lod_constant(entry, triangle);
    ray::material_t material = trace_data.materials[entry->material_index];

    // potentially limit ray tmax
    float tmax_inside_volume = 2 * length(entry->transform.scale() * entry->aabb.half_extents());

    float3 V = -WorldRayDirection();
    float NoV = abs(dot(V, v.normal));

    payload.entity_index = InstanceIndex();
    payload.position = v.position;
    payload.normal = v.normal;

    // participating media
    bool sample_medium = false;
    float3 sigma_t = payload.media.sigma_s + payload.media.sigma_a;

    bool use_direct_lighting = trace_data.sunlight_params.intensity > 0.0;

    if(any(sigma_t > float3(0)))
    {
        float t = 0;
        int channel = int(min(utils::rnd(rng_state) * 3, 2));
        const bool homogenous_medium = true;

        float3 sigma_s = payload.media.sigma_s;

        if(homogenous_medium)
        {
            t = min(-log(1 - utils::rnd(rng_state)) / max(sigma_t[channel], ray::EPS), RayTCurrent());
            sample_medium = t < RayTCurrent();

            float3 beam_tr = exp(-sigma_t * t);
            float3 density = sample_medium ? beam_tr * sigma_t : beam_tr;
            float pdf = max(channel_avg(density), ray::PDF_EPS);
            payload.beta *= sample_medium ? (sigma_s * beam_tr / pdf) : (beam_tr / pdf);
        }
        else
        {
            // NOTE: wip grid-density media with monochromatic attenuation
            const float inv_max_density = 1.0;
            utils::transform_t to_media = to_aabb_norm(entry);

            ray::Ray ray_local = payload.ray;
            float scale = length(float3(to_media.scale_x, to_media.scale_y, to_media.scale_z));
            ray_local.origin = utils::apply_transform(to_media, ray_local.origin);
            ray_local.direction = scale * utils::apply_rotation(to_media, ray_local.direction);

            while(true)
            {
                t -= log(1 - utils::rnd(rng_state)) * inv_max_density / sigma_t[channel];
                if(t >= RayTCurrent()) { break; }

                float3 p = ray_local.origin + t * ray_local.direction;
                float density =
                        clamp(0.5 * (utils::simplex(float4(4 * p, trace_data.params.time * 0.1)) + 1.0), 0.0, 1.0);
                if(density * inv_max_density > utils::rnd(rng_state))
                {
                    sample_medium = true;
                    break;
                }
            }
        }

        if(sample_medium)
        {
            const float g = payload.media.phase_g;
            float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));
            payload.ray.origin += payload.ray.direction * t;

            if(use_direct_lighting)
            {
                payload.radiance += payload.beta * ray::sample_sun_light_phase(payload.ray, trace_data.sunlight_params,
                                                                               topLevelAS, g, rng_state);
            }

            float phase_pdf;
            payload.ray.direction =
                    mul(utils::sample_phase_hg(Xi, g, phase_pdf), utils::local_frame(payload.ray.direction));
        }
    }

    if(!sample_medium) { payload.ray.origin = payload.position; }

    bool sample_surface = !(material.null_surface || sample_medium);
    bool backface = (HitKind() == HIT_KIND_BACK_FACE);
    float eta = backface ? material.ior / payload.last_ior : payload.media.ior / material.ior;
    eta += ray::EPS;

    sigma_t = -log(material.attenuation_color.rgb) / material.attenuation_distance;
    sigma_t = backface ? float3(0) : sigma_t;
    payload.media.sigma_s = material.scattering_ratio * sigma_t;
    payload.media.sigma_a = (1 - material.scattering_ratio) * sigma_t;
    payload.media.phase_g = material.phase_asymmetry_g;
    payload.media.ior = sample_surface ? material.ior : payload.media.ior;

    // determine next media-op
    payload.media_op = sample_medium ? ray::MediaOp::NoOp : (backface ? ray::MediaOp::Leave : ray::MediaOp::Enter);

    if(sample_surface)
    {
        if((material.texture_type_flags & uint(ray::TextureTypeBits::Color)) != 0)
        {
            material.color *= sample_texture_lod(u_textures[NonUniformResourceIndex(material.albedo_index)],
                                                 v.tex_coord, NoV, payload.cone.width, triangle_lod);
        }
        material.color = trace_data.params.disable_material ? float4(float3(0.8), 1.0) : material.color;

        if((material.texture_type_flags & uint(ray::TextureTypeBits::Normal)) != 0)
        {
            float3 normal;
            normal.xy = 2.0 * (sample_texture_lod(u_textures[NonUniformResourceIndex(material.normalmap_index)],
                                                  v.tex_coord, NoV, payload.cone.width, triangle_lod)
                                       .xy -
                               float2(0.5));
            normal.z = sqrt(max(0.0, 1.0 - normal.x * normal.x - normal.y * normal.y));

            float3 b = normalize(cross(v.normal, v.tangent));
            normal = normalize(mul(normal, float3x3(v.tangent, b, v.normal)));

            // hemisphere clamp
            payload.normal = normalize(normal + max(0, -dot(normal, v.normal) + 1e-3) * v.normal);
        }

        float3 ff_normal = faceforward(payload.normal, WorldRayDirection(), payload.normal);

        if((material.texture_type_flags & uint(ray::TextureTypeBits::Emission)) != 0)
        {
            float3 emmision_tex = sample_texture_lod(u_textures[NonUniformResourceIndex(material.emission_index)],
                                                     v.tex_coord, NoV, payload.cone.width, triangle_lod)
                                          .rgb;
            if(any(material.emission.rgb > 0.0)) { material.emission.rgb *= emmision_tex; }
            else
            {
                material.emission.rgb = emmision_tex;
            }
        }
        material.emission.rgb *= dot(payload.normal, ff_normal) > 0 ? material.emission.a : 0.0;
        payload.radiance += payload.beta * material.emission.rgb;

        if((material.texture_type_flags & uint(ray::TextureTypeBits::AoRoughMetal)) != 0)
        {
            float2 rough_metal = sample_texture_lod(u_textures[NonUniformResourceIndex(material.ao_rough_metal_index)],
                                                    v.tex_coord, NoV, payload.cone.width, triangle_lod)
                                         .gb;
            material.roughness *= rough_metal.x;
            material.metalness *= rough_metal.y;
        }

        if((material.texture_type_flags & uint(ray::TextureTypeBits::Transmission)) != 0)
        {
            material.transmission *=
                    sample_texture_lod(u_textures[NonUniformResourceIndex(material.transmission_index)], v.tex_coord,
                                       NoV, payload.cone.width, triangle_lod)
                            .x;
        }

        payload.cone = propagate(payload.cone, 0.0, RayTCurrent());

        // increase material-roughness for longer paths, reduce fireflies
        //material.roughness = pow(material.roughness, 1.0 / (1.0 + payload.depth));

        if(trace_data.params.restir_gi != 0 && payload.depth == 0)
        {
            payload.vis_material = material;
            payload.vis_eta = eta;
        }

        ray::bsdf_sample_t bsdf_sample = ray::sample_disney(material, payload.normal, V, eta, rng_state);

        if(use_direct_lighting)
        {
            payload.radiance +=
                    payload.beta * ray::sample_sun_light(material, trace_data.sunlight_params, topLevelAS,
                                                         payload.position, payload.normal, V, eta, rng_state);
        }

        if(bsdf_sample.pdf <= 0.0)
        {
            payload.stop = true;
            return;
        }

        payload.ray.direction = bsdf_sample.direction;
        float cos_theta = abs(dot(payload.normal, bsdf_sample.direction));

        float3 path_throughput = bsdf_sample.F * cos_theta / max(bsdf_sample.pdf, ray::PDF_EPS);

        // optionally clamp path-throughput to suppress fireflies
        if(payload.depth >= 1 && trace_data.params.max_path_beta > 0.0)
        {
            path_throughput = min(path_throughput, trace_data.params.max_path_beta);
        }
        payload.beta *= path_throughput;

        if(bsdf_sample.transmission)
        {
            // examine next media-op, limit ray.tmax when entering a volume
            payload.ray.tmax = payload.media_op == ray::MediaOp::Enter ? tmax_inside_volume : ray::DEFAULT_TMAX;
        }
        payload.media_op = bsdf_sample.transmission ? payload.media_op : ray::MediaOp::NoOp;
    }

    // Russian roulette
    if(max3(payload.beta) <= 0.05 && payload.depth >= 1)
    {
        float q = 1.0 - max3(payload.beta);
        if(utils::rnd(rng_state) < q)
        {
            payload.stop = true;
            return;
        }
        payload.beta /= (1.0 - q);
    }

    payload.rng_state = rng_state;
}

//-----------------------------------------------------------------------------
// any-hit
//-----------------------------------------------------------------------------

[shader("anyhit")]
void any_hit(inout ray::payload_t payload, BuiltInTriangleIntersectionAttributes attribs)
{
    uint rng_state = payload.rng_state;

    let entry = Ptr<ray::entry_t, Access.Read>(trace_data.entries + InstanceIndex());
    ray::material_t material = trace_data.materials[entry->material_index];

    if(!material.null_surface &&
       (material.blend_mode == ray::BlendMode::Mask || material.blend_mode == ray::BlendMode::Blend))
    {
        if((material.texture_type_flags & uint(ray::TextureTypeBits::Color)) != 0)
        {
            uint base = entry->base_index + 3 * PrimitiveIndex();
            int3 ind = int3(trace_data.index_buffers[entry->buffer_index][base + 0],
                            trace_data.index_buffers[entry->buffer_index][base + 1],
                            trace_data.index_buffers[entry->buffer_index][base + 2]);

            ray::Triangle t = ray::Triangle(
                    utils::unpack(trace_data.vertex_buffers[entry.buffer_index][entry.vertex_offset + ind.x]),
                    utils::unpack(trace_data.vertex_buffers[entry.buffer_index][entry.vertex_offset + ind.y]),
                    utils::unpack(trace_data.vertex_buffers[entry.buffer_index][entry.vertex_offset + ind.z]));

            float3 barycentric = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x,
                                        attribs.barycentrics.y);
            float2 tex_coord =
                    t.v0.tex_coord * barycentric.x + t.v1.tex_coord * barycentric.y + t.v2.tex_coord * barycentric.z;
            material.color *= u_textures[NonUniformResourceIndex(material.albedo_index)].SampleLevel(tex_coord, 0);
        }
        material.color = trace_data.params.disable_material ? float4(float3(0.8), 1.0) : material.color;

        if(material.blend_mode == ray::BlendMode::Mask && material.color.a < material.alpha_cutoff) { IgnoreHit(); }
        if(material.blend_mode == ray::BlendMode::Blend && material.color.a < utils::rnd(rng_state)) { IgnoreHit(); }
    }
}

//-----------------------------------------------------------------------------
// miss shaders
//-----------------------------------------------------------------------------

[shader("miss")]
void miss_default(inout ray::payload_t payload)
{
    payload.stop = true;
    payload.normal = float3(0.0);
    payload.position = float3(0.0);

    float3 col = utils::environment_white(payload.ray.direction);
    payload.radiance += trace_data.params.environment * payload.beta * col;
}

[shader("miss")]
void miss_environment(inout ray::payload_t payload)
{
    uint w, h;
    u_sampler_cube.GetDimensions(w, h);
    float lod = 0.5 * log2(float(w) * float(h));
    lod += log2(abs(payload.cone.width));

    payload.stop = true;

    payload.radiance +=
            trace_data.params.environment * payload.beta * u_sampler_cube.SampleLevel(WorldRayDirection(), lod).rgb;
}
