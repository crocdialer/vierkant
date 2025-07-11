#ifndef RAY_VISIBILITY_TEST_GLSL
#define RAY_VISIBILITY_TEST_GLSL

#extension GL_EXT_ray_query : enable

#include "ray_common.glsl"

//! group information for a ray-hit
struct ray_hit_t
{
    uint entry_index;
    uint primitive_index;
    vec2 barycentrics;
    float hit_t;
    bool front_face;
    bool valid;
};

ray_hit_t ray_cast(Ray ray, accelerationStructureEXT as)
{
    const uint ray_flags =  gl_RayFlagsOpaqueEXT;

    ray_hit_t ret;
    ret.valid = false;

    rayQueryEXT query;
    rayQueryInitializeEXT(query, as, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, ray.origin, ray.tmin, ray.direction, ray.tmax);
    rayQueryProceedEXT(query);

    if(rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
    {
        const bool committed = true;
        ret.entry_index = rayQueryGetIntersectionInstanceCustomIndexEXT(query, committed);
        ret.primitive_index = rayQueryGetIntersectionPrimitiveIndexEXT(query, committed);
        ret.barycentrics = rayQueryGetIntersectionBarycentricsEXT(query, committed);
        ret.hit_t = rayQueryGetIntersectionTEXT(query, committed);
        ret.front_face = rayQueryGetIntersectionFrontFaceEXT(query, committed);
        ret.valid = true;
    }
    // define a return-struct and group values above
    return ret;
}

bool visibility_test(Ray ray, accelerationStructureEXT as)
{
    const uint ray_flags =  gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT;
    rayQueryEXT query;
    rayQueryInitializeEXT(query, as, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, ray.origin, ray.tmin, ray.direction, ray.tmax);
    rayQueryProceedEXT(query);
    return rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionNoneEXT;
}

#endif // RAY_VISIBILITY_TEST_GLSL