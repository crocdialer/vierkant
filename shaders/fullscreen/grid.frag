#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../utils/color_cast.glsl"
#include "../utils/sampling.glsl"
#include "../renderer/types.glsl"

struct grid_params_t
{
    mat4 projection_view;
    mat4 projection_inverse;
    mat4 view_inverse;
    vec4 plane;
    uint color;
    uint color_x;
    uint color_z;
    float dist;
    vec2 line_width;
    bool ortho;
    bool axis;
};

layout(std140, binding = 0) uniform UBO { grid_params_t grid_params; };

layout(push_constant) uniform PushConstants
{
    render_context_t context;
};

void camera_ray(vec2 frag_coord, out vec3 ray_origin, out vec3 ray_direction)
{
    vec2 uv_coord = frag_coord / context.size;
    vec2 d = uv_coord * 2.0 - 1.0;

    if(grid_params.ortho)
    {
        ray_origin = (grid_params.view_inverse * grid_params.projection_inverse * vec4(d.x, d.y, 1, 1)).xyz;
        ray_direction = -grid_params.view_inverse[2].xyz;
    }
    else
    {
        // ray origin
        ray_origin = grid_params.view_inverse[3].xyz;

        // ray direction
        ray_direction = normalize(mat3(grid_params.view_inverse) * (grid_params.projection_inverse * vec4(d.x, d.y, 1, 1)).xyz);
    }
}

// Pristine grid from The Best Darn Grid Shader (yet)
// https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8
// version with explicit gradients for use with raycast shaders like this one
float pristine_grid(in vec2 uv, in vec2 ddx, in vec2 ddy, vec2 line_width)
{
    vec2 uvDeriv = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y)));
    bvec2 invertLine = bvec2(line_width.x > 0.5, line_width.y > 0.5);
    vec2 targetWidth = vec2(invertLine.x ? 1.0 - line_width.x : line_width.x,
                            invertLine.y ? 1.0 - line_width.y : line_width.y);
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = uvDeriv * 1.5;
    vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);
    gridUV.x = invertLine.x ? gridUV.x : 1.0 - gridUV.x;
    gridUV.y = invertLine.y ? gridUV.y : 1.0 - gridUV.y;
    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2.x = invertLine.x ? 1.0 - grid2.x : grid2.x;
    grid2.y = invertLine.y ? 1.0 - grid2.y : grid2.y;
    return mix(grid2.x, 1.0, grid2.y);
}

//const float N = 10.0; // grid ratio
float grid_grad_box(in vec2 uv, vec2 ddx, vec2 ddy, float grid_ratio)
{
    // filter kernel
    vec2 w = max(abs(ddx), abs(ddy)) + 0.01;

    // analytic (box) filtering
    vec2 a = uv + 0.5 * w;
    vec2 b = uv - 0.5 * w;
    vec2 i = (floor(a) + min(fract(a) * grid_ratio, 1.0) - floor(b) - min(fract(b) * grid_ratio, 1.0)) / (grid_ratio * w);

    // pattern
    return 1.0 - (1.0 - i.x) * (1.0 - i.y);
}

bool intersect_plane(const vec4 plane, const vec3 ray_origin, const vec3 ray_direction, out vec3 pos)
{
    float denom = dot(ray_direction, -plane.xyz);

    // ray is parallel to the plane
    if (abs(denom) < 1e-6){ return false; }

    // compute intersection distance
    float d = (plane.w - dot(ray_origin, -plane.xyz)) / denom;

    pos = ray_origin + d * ray_direction;

    // intersection not behind ray_origin
    return d >= 0.0;
}

layout(location = 0) in VertexData
{
    vec2 tex_coord;
} vertex_in;

layout(location = 0) out vec4 out_color;

void main()
{
    // create pixel-ray and ray-differentials
    vec3 ray_origin;
    vec3 ray_direction;
    vec3 ray_origin_ddx;
    vec3 ray_direction_ddx;
    vec3 ray_origin_ddy;
    vec3 ray_direction_ddy;

    camera_ray(gl_FragCoord.xy, ray_origin, ray_direction);
    camera_ray(gl_FragCoord.xy + vec2(1, 0), ray_origin_ddx, ray_direction_ddx);
    camera_ray(gl_FragCoord.xy + vec2(0, 1), ray_origin_ddy, ray_direction_ddy);

    // intersect with groundplane, get grid-uv
    vec3 pos;
    if(intersect_plane(grid_params.plane, ray_origin, ray_direction, pos))
    {
        vec3 n = grid_params.plane.xyz;
        float d_inv = 1.0 / grid_params.dist;

        mat3 frame = transpose(local_frame(n));
        vec2 grid_uv = d_inv * (frame * pos).xy;

        // compute ray differentials
        vec3 ddx_pos = ray_origin_ddx - ray_direction_ddx * dot(ray_origin_ddx - pos, n) / dot(ray_direction_ddx, n);
        vec3 ddy_pos = ray_origin_ddy - ray_direction_ddy * dot(ray_origin_ddy - pos, n) / dot(ray_direction_ddy, n);

        // texture sampling footprint
        vec2 ddx_uv = d_inv * (frame * ddx_pos).xy - grid_uv;
        vec2 ddy_uv = d_inv * (frame * ddy_pos).xy - grid_uv;

        // grid coverage
        float coverage = pristine_grid(grid_uv, ddx_uv, ddy_uv, grid_params.line_width);
        vec4 color = color_cast(grid_params.color);

        if(grid_params.axis)
        {
            color = mix(color_cast(grid_params.color_x), color, smoothstep(0.0, grid_params.line_width.y, abs(grid_uv.y)));
            color = mix(color_cast(grid_params.color_z), color, smoothstep(0.0, grid_params.line_width.x, abs(grid_uv.x)));
        }
        out_color = vec4(color.rgb, color.a * coverage);

        // not very elegant but yeah, works
        vec4 proj = grid_params.projection_view * vec4(pos, 1.0);
        gl_FragDepth = coverage > 0.0 ? proj.z / proj.w : 0.0;
    }
    else
    {
        out_color = vec4(0);

        // infinite far
        gl_FragDepth = 0.0;
    }
}