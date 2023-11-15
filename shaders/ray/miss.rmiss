#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_common.glsl"
#include "../utils/sdf.glsl"
#include "../utils/procedural_environment.glsl"

layout(location = 0) rayPayloadInEXT payload_t payload;

layout(std140, binding = 9) uniform ubo_t
{
    float environment_factor;
} ubo;

float sdCross3(in vec3 p, vec3 sz)
{
    return min(min(sdBox(p, sz), sdBox(p, sz.zyx)), sdBox(p, sz.xzy));
}

float map(in vec3 p, float time)
{
    p = mod(p, 1.5) - 0.75;

    // some sphere
    float d = sd_sphere(p, 0.12);

    // substract a box
    d = max(d, - sdCross3(p, vec3(0.05, 0.05, 0.5)));
    return d;
}

// http://iquilezles.org/www/articles/normalsSDF/normalsSDF.htm
vec3 calc_normal(in vec3 pos, in float time)
{
    const vec2 e = vec2(1.0, -1.0) * 0.5773;
    const float h = 0.00025;

    return normalize(e.xyy*map(pos + e.xyy * h, time) +
                     e.yyx*map(pos + e.yyx * h, time) +
                     e.yxy*map(pos + e.yxy * h, time) +
                     e.xxx*map(pos + e.xxx * h, time));
}

#define MAX_STEPS 128
#define MARCH_EPS .01
#define MAX_DISTANCE 100.0

// return: march_distance, min_distance
vec2 march(Ray ray)
{
    float march_distance = 0.;

    // keep track of minimum distance
    float min_distance = MAX_DISTANCE;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        vec3 p = ray.origin + ray.direction * march_distance;
        float distance = map(p, 0.0);

        min_distance = min(min_distance, distance);

        // march max step
        march_distance += distance;

        if (march_distance >= MAX_DISTANCE || distance < MARCH_EPS) break;
    }
    return vec2(march_distance, min_distance);
}

#define PI 3.1415926535897932384626433832795

// low-life skycolor routine
vec3 sky_color(vec3 direction)
{
    // sun angular diameter
    const float sun_angle =  0.524167 *  PI / 180.0;
    const vec3 sun_color = vec3(1.0, 0.5, 0.1);
    const float sun_intensity = 60.0;
    const vec3 color_up = vec3(0.25f, 0.5f, 1.0f) * 1;

    vec3 col = mix(vec3(0.9f), color_up, max(vec3(0), direction.y));

    const vec3 sun_dir = normalize(vec3(.4, 1.0, 0.7));
    float sun = clamp(dot(direction, sun_dir), 0.0, 1.0);

    col += cos(sun_angle) < sun ? sun_intensity * sun_color : vec3(0);
    return max(vec3(0.0), col);
}

void main()
{
    // stop path tracing loop from rgen shader
    payload.stop = true;
    payload.normal = vec3(0.);
    payload.position = vec3(0.);

//    float d = march(payload.ray).x;
//    vec3 p = payload.ray.origin + payload.ray.direction * d;
//    vec3 n = calc_normal(p, 0.0);

    vec3 col = environment_white(payload.ray.direction);
    payload.radiance += ubo.environment_factor * payload.beta * col;
}