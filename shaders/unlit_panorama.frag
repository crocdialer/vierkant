#version 460
#extension GL_ARB_separate_shader_objects : enable

#define MAX_NUM_DRAWABLES 4096

#define ONE_OVER_PI 0.31830988618379067153776752674503

#define COLOR 0

layout(binding = 1) uniform sampler2D u_sampler_2D[1];

layout(location = 0) in VertexData
{
    vec3 eye_vec;
} vertex_in;

layout(location = 0) out vec4 out_color;

// map normalized direction to equirectangular texture coordinate
vec2 panorama(vec3 ray)
{
    return vec2(0.5 + 0.5 * atan(ray.x, -ray.z) * ONE_OVER_PI, acos(ray.y) * ONE_OVER_PI);
}

void main()
{
    out_color = texture(u_sampler_2D[COLOR], panorama(normalize(vertex_in.eye_vec)));
}