#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../utils/transform.glsl"
#include "../renderer/types.glsl"

struct grid_params_t
{
    mat4 projection_view;
    mat4 projection_inverse;
    mat4 view_inverse;
    vec2 line_width;
    bool ortho;
};

layout(std140, binding = 0) uniform UBO { grid_params_t grid_params; };

layout(push_constant) uniform PushConstants
{
    render_context_t context;
};

void camera_ray(vec2 frag_coord, inout vec3 ray_origin, inout vec3 ray_direction)
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
float pristine_grid(in vec2 uv, vec2 lineWidth)
{
    vec2 ddx = dFdx(uv);
    vec2 ddy = dFdy(uv);
    vec2 uvDeriv = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y)));
    bvec2 invertLine = bvec2(lineWidth.x > 0.5, lineWidth.y > 0.5);
    vec2 targetWidth = vec2(
    invertLine.x ? 1.0 - lineWidth.x : lineWidth.x,
    invertLine.y ? 1.0 - lineWidth.y : lineWidth.y
    );
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

// version with explicit gradients for use with raycast shaders like this one
float pristineGrid(in vec2 uv, in vec2 ddx, in vec2 ddy, vec2 lineWidth)
{
    vec2 uvDeriv = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y)));
    bvec2 invertLine = bvec2(lineWidth.x > 0.5, lineWidth.y > 0.5);
    vec2 targetWidth = vec2(
    invertLine.x ? 1.0 - lineWidth.x : lineWidth.x,
    invertLine.y ? 1.0 - lineWidth.y : lineWidth.y
    );
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

bool intersect_ground(const vec3 ray_origin, const vec3 ray_direction, out vec3 pos)
{
    // ray is parallel to the plane
    if (abs(ray_direction.y) < 1e-6){ return false; }

    // compute intersection distance
    float dist = -ray_origin.y / ray_direction.y;

    pos = ray_origin + dist * ray_direction;

    // intersection not behind ray_origin
    return dist >= 0.0;
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
    if(intersect_ground(ray_origin, ray_direction, pos))
    {
        vec2 grid_uv = pos.xz;
        float coverage = pristine_grid(grid_uv, grid_params.line_width);
        out_color = vec4(vec3(1), coverage);

        // not very elegant
        vec4 proj = grid_params.projection_view * vec4(pos, 1.0);
        gl_FragDepth = proj.z / proj.w;
    }
    else
    {
        out_color = vec4(0);

        // infinite far
        gl_FragDepth = 0.0;
    }
}