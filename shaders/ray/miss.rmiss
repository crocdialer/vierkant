#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"

layout(location = 0) rayPayloadInEXT payload_t payload;

// http://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sd_sphere(in vec3 p, in float r)
{
    return length(p) - r;
}

float map(in vec3 p, float time)
{
    p = mod(p, 1.5) - 0.5;

    // some sphere
    float d = sd_sphere(p, 0.12);
    return d;
}

// http://iquilezles.org/www/articles/normalsSDF/normalsSDF.htm
vec3 calc_normal(in vec3 pos, in float time)
{
    vec2 e = vec2(1.0, -1.0) * 0.2;//0.5773;
    const float eps = 0.0001;//0.00025;

    return normalize(e.xyy*map(pos + e.xyy * eps, time) +
                     e.yyx*map(pos + e.yyx * eps, time) +
                     e.yxy*map(pos + e.yxy * eps, time) +
                     e.xxx*map(pos + e.xxx * eps, time));
}

#define MAX_STEPS 128
#define MARCH_EPS .001
#define MAX_DISTANCE 100.0

float march(Ray ray)
{
    float march_distance = 0.;//Distane Origin

    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec3 p = ray.origin + ray.direction * march_distance;
        float distance = map(p, 0.0);

        // march max step
        march_distance += distance;

        if (march_distance > MAX_DISTANCE || distance < MARCH_EPS) break;
    }
    return march_distance;
}

// low-life skycolor routine
vec3 sky_color(vec3 direction)
{
    const vec3 color_up = vec3(0.25f, 0.5f, 1.0f) * 2;

    return mix(mix(vec3(2.0f), color_up, direction.y),
               mix(vec3(2.0f), vec3(0.2f), 4 * -direction.y), direction.y > 0.0 ? 0.0 : 1.0);
}

void main()
{
    // stop path tracing loop from rgen shader
    payload.stop = true;
    payload.normal = vec3(0.);
    payload.position = vec3(0.);

    float d = march(payload.ray);
    vec3 p = payload.ray.origin + payload.ray.direction * d;
    vec3 n = calc_normal(p, 0.0);

    payload.radiance += payload.beta * sky_color(reflect(payload.ray.direction, n));
//    payload.radiance += payload.beta * n;
}