module visibility_test;

import ray_common;

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

public bool visibility_test(Ray ray, RaytracingAccelerationStructure as)
{
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
    query.TraceRayInline(as, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF,
                         RayDesc(ray.origin, ray.tmin, ray.direction, ray.tmax));
    query.Proceed();
    return query.CommittedStatus() == COMMITTED_NOTHING;
}

}// namespace ray
