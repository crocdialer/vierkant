#ifndef RAY_VISIBILITY_TEST_GLSL
#define RAY_VISIBILITY_TEST_GLSL

#extension GL_EXT_ray_query : enable

#include "ray_common.glsl"

bool visibility_test(Ray ray, accelerationStructureEXT as)
{
    const uint ray_flags =  gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT;
    float tmin = 0.0;
    float tmax = 10000.0;

    rayQueryEXT query;
    rayQueryInitializeEXT(query, as, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, ray.origin, tmin, ray.direction, tmax);
    rayQueryProceedEXT(query);
    return rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionNoneEXT;
}

#endif // RAY_VISIBILITY_TEST_GLSL