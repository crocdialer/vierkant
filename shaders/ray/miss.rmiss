#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

layout(location = 0) rayPayloadInEXT payload_t payload;

// Returns the color of the sky in a given direction (in linear color space)
vec3 sky_color(vec3 direction)
{
//    if(direction.y > 0.0f)
//    {
//        return mix(vec3(1.0f), vec3(0.25f, 0.5f, 1.0f), direction.y);
//    }
//    return vec3(0.03f);

    const vec3 color_up = vec3(0.25f, 0.5f, 1.0f);

    return mix(mix(vec3(1.0f), color_up, direction.y),
               mix(vec3(1.0f), vec3(0.2f), 4 * -direction.y), direction.y > 0.0 ? 0.0 : 1.0);
}

void main()
{
    // stop path tracing loop from rgen shader
    payload.stop = true;
    payload.normal = vec3(0.);
    payload.position = vec3(0.);
    payload.radiance += payload.beta * sky_color(gl_WorldRayDirectionEXT);
}