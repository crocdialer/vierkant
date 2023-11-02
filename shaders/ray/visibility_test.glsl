#ifndef RAY_VISIBILITY_TEST_GLSL
#define RAY_VISIBILITY_TEST_GLSL

#include "ray_common.glsl"

layout(location = MISS_INDEX_SHADOW) rayPayloadEXT shadow_payload_t payload_shadow;

bool visibility_test(Ray ray, accelerationStructureEXT as)
{
    const uint ray_flags =  gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT;
    float tmin = 0.0;
    float tmax = 10000.0;
    payload_shadow.shadow = true;

    traceRayEXT(as,                 // acceleration structure
                ray_flags,          // rayflags
                0xff,               // cullMask
                0,                  // sbtRecordOffset
                0,                  // sbtRecordStride
                MISS_INDEX_SHADOW,  // missIndex
                ray.origin,         // ray origin
                tmin,               // ray min range
                ray.direction,      // ray direction
                tmax,               // ray max range
                MISS_INDEX_SHADOW); // payload-location
    return !payload_shadow.shadow;
}

#endif // RAY_VISIBILITY_TEST_GLSL