#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../utils/transform.glsl"
#include "../renderer/types.glsl"

struct camera_t
{
    mat4 projection_inverse;
    mat4 view_inverse;
    bool ortho;
};

layout(std140, binding = 0) uniform UBO { camera_t cam; };

layout(push_constant) uniform PushConstants
{
    render_context_t context;
};

void camera_ray(vec2 uv_coord, inout vec3 ray_origin, inout vec3 ray_direction)
{
    vec2 d = uv_coord * 2.0 - 1.0;

    if(cam.ortho)
    {
        ray_origin = (cam.view_inverse * cam.projection_inverse * vec4(d.x, d.y, 1, 1)).xyz;
        ray_direction = -cam.view_inverse[2].xyz;
    }
    else
    {
        // ray origin
        ray_origin = cam.view_inverse[3].xyz;

        // ray direction
        ray_direction = normalize(mat3(cam.view_inverse) * (cam.projection_inverse * vec4(d.x, d.y, 1, 1)).xyz);
    }
}

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
//    gl_FragDepth = ;
    // ray onto ground-plane, get uv
    vec3 ray_origin;
    vec3 ray_direction;

    camera_ray(vertex_in.tex_coord, ray_origin, ray_direction);

    out_color = vec4(abs(ray_direction.xy), 0, 1.0);//params.color * texture(u_sampler_2D[COLOR], vertex_in.tex_coord);
}