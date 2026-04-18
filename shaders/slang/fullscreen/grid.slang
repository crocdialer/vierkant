import "../renderer/types.slang";
import "../utils/color_cast.slang";
import "../utils/sampling.slang";

struct grid_params_t
{
    float4x4 projection_view;
    float4x4 projection_inverse;
    float4x4 view_inverse;
    float4 plane;
    uint color;
    uint color_x;
    uint color_z;
    float dist;
    float2 line_width;
    bool ortho;
    bool axis;
};

[[vk::binding(0, 0)]] ConstantBuffer<grid_params_t> grid_params;

[[vk::push_constant]]
renderer::context_t context;

void camera_ray(float2 frag_coord, out float3 ray_origin, out float3 ray_direction)
{
    float2 uv = frag_coord / context.size;
    float2 d = uv * 2.0 - 1.0;

    if(grid_params.ortho)
    {
        ray_origin = mul(grid_params.view_inverse, mul(grid_params.projection_inverse, float4(d.x, d.y, 1, 1))).xyz;
        ray_direction = -float3(grid_params.view_inverse[0][2],
                                grid_params.view_inverse[1][2],
                                grid_params.view_inverse[2][2]);
    }
    else
    {
        ray_origin = mul(grid_params.view_inverse, float4(0, 0, 0, 1)).xyz;
        float3x3 view_rot = float3x3(grid_params.view_inverse[0].xyz,
                                     grid_params.view_inverse[1].xyz,
                                     grid_params.view_inverse[2].xyz);
        ray_direction = normalize(mul(view_rot, normalize(mul(grid_params.projection_inverse, float4(d.x, d.y, 1, 1)).xyz)));
    }
}

float pristine_grid(float2 uv, float2 ddx, float2 ddy, float2 line_width)
{
    float2 uvDeriv = float2(length(float2(ddx.x, ddy.x)), length(float2(ddx.y, ddy.y)));
    bool2 invertLine = bool2(line_width.x > 0.5, line_width.y > 0.5);
    float2 targetWidth = float2(invertLine.x ? 1.0 - line_width.x : line_width.x,
                                invertLine.y ? 1.0 - line_width.y : line_width.y);
    float2 drawWidth = clamp(targetWidth, uvDeriv, float2(0.5));
    float2 lineAA = uvDeriv * 1.5;
    float2 gridUV = abs(frac(uv) * 2.0 - 1.0);
    gridUV.x = invertLine.x ? gridUV.x : 1.0 - gridUV.x;
    gridUV.y = invertLine.y ? gridUV.y : 1.0 - gridUV.y;
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = lerp(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2.x = invertLine.x ? 1.0 - grid2.x : grid2.x;
    grid2.y = invertLine.y ? 1.0 - grid2.y : grid2.y;
    return lerp(grid2.x, 1.0, grid2.y);
}

bool intersect_plane(float4 plane, float3 ray_origin, float3 ray_direction, out float3 pos)
{
    float denom = dot(ray_direction, -plane.xyz);
    if(abs(denom) < 1e-6) { return false; }
    float d = (plane.w - dot(ray_origin, -plane.xyz)) / denom;
    pos = ray_origin + d * ray_direction;
    return d >= 0.0;
}

struct FragOutput
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

[[shader("pixel")]]
FragOutput fragment_main(float4 sv_position : SV_Position)
{
    float3 ray_origin, ray_direction;
    float3 ray_origin_ddx, ray_direction_ddx;
    float3 ray_origin_ddy, ray_direction_ddy;

    camera_ray(sv_position.xy, ray_origin, ray_direction);
    camera_ray(sv_position.xy + float2(1, 0), ray_origin_ddx, ray_direction_ddx);
    camera_ray(sv_position.xy + float2(0, 1), ray_origin_ddy, ray_direction_ddy);

    FragOutput o;
    float3 pos;

    if(intersect_plane(grid_params.plane, ray_origin, ray_direction, pos))
    {
        float3 n = grid_params.plane.xyz;
        float d_inv = 1.0 / grid_params.dist;

        float3x3 frame = utils::local_frame(n);
        float2 grid_uv = d_inv * mul(frame, pos).xy;

        float3 ddx_pos = ray_origin_ddx - ray_direction_ddx * dot(ray_origin_ddx - pos, n) / dot(ray_direction_ddx, n);
        float3 ddy_pos = ray_origin_ddy - ray_direction_ddy * dot(ray_origin_ddy - pos, n) / dot(ray_direction_ddy, n);

        float2 ddx_uv = d_inv * mul(frame, ddx_pos).xy - grid_uv;
        float2 ddy_uv = d_inv * mul(frame, ddy_pos).xy - grid_uv;

        float coverage = pristine_grid(grid_uv, ddx_uv, ddy_uv, grid_params.line_width);
        float4 color = utils::color_cast(grid_params.color);

        if(grid_params.axis)
        {
            color = lerp(utils::color_cast(grid_params.color_x), color, smoothstep(0.0, grid_params.line_width.y, abs(grid_uv.y)));
            color = lerp(utils::color_cast(grid_params.color_z), color, smoothstep(0.0, grid_params.line_width.x, abs(grid_uv.x)));
        }
        o.color = float4(color.rgb, color.a * coverage);

        float4 proj = mul(grid_params.projection_view, float4(pos, 1.0));
        o.depth = coverage > 0.0 ? proj.z / proj.w : 0.0;
    }
    else
    {
        o.color = float4(0);
        o.depth = 0.0;
    }
    return o;
}
