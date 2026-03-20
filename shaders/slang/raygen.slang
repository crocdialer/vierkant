module raygen;

import "ray/ray_common";
import "ray/pixel_data";
import "utils/random";
import "utils/hash";
import "utils/sampling";
import "utils/constants";

static const uint MAX_MEDIA_STACK_SIZE = 4;

[[vk::binding(0, 0)]]
RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 0)]]
ConstantBuffer<ray::trace_data_t> trace_data;

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
        direction = -trace_data.camera.view_inverse[2].xyz;
    }
    else
    {
        // lens-offset and ray-origin
        float3 lens_offset = float3(trace_data.camera.aperture / 2.0 * utils::sample_unit_disc(Xi), 0.0);
        origin = mul(trace_data.camera.view_inverse, float4(lens_offset, 1)).xyz;

        // ray direction
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
    r.tmax = 10000.0;
    return r;
}

trace_result_t trace(uint2 coord, uint2 size)
{
    trace_result_t result;

    // init random number generator
    uint rng_state = utils::xxhash32(trace_data.params.random_seed, size.x * coord.y + coord.x);

    result.radiance = float3(0);
    result.coverage = 0.0;

    float sample_offset = 2.0 * utils::rnd(rng_state) - 1.0;

    // cone spread_angle
    float spread_angle = atan(2.0 * tan(trace_data.camera.fov / 2.0) / float(size.y));

    for(int sampleIdx = 0; sampleIdx < int(trace_data.params.num_samples); sampleIdx++)
    {
        // combine random and stratified sampling
        float2 rnd_coord = float2(coord) +
                           frac(utils::Hammersley(sampleIdx, trace_data.params.num_samples) + float2(sample_offset));
        float2 Xi = float2(utils::rnd(rng_state), utils::rnd(rng_state));

        ray::Ray next_ray = camera_ray(rnd_coord, float2(size), Xi);

        ray::RayCone cone;
        cone.spread_angle = spread_angle;
        cone.width = 0.0;

        // start in camera-defined medium
        uint media_index = 0;
        ray::media_t media[MAX_MEDIA_STACK_SIZE];
        media[media_index] = trace_data.camera_media;

        float3 beta = float3(1);
        float3 path_radiance = float3(0);

        ray::payload_t payload;

        // trace a path
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

    // store pixel
    trace_data.out_pixels[buffer_idx] =
            ray::pack(accumulated_radiance.rgb, result.coverage, result.normal, result.depth, result.entity_index);
}
