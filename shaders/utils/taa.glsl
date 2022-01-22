#include "color_ycc.glsl"

struct taa_ubo_t
{
    mat4 current_vp;
    mat4 current_inverse_vp;
    mat4 previous_vp;
    float near;
    float far;
    vec2 sample_offset;
};

vec2 reproject(vec2 coord,
               float depth,
               mat4 current_inverse_vp,
               mat4 previous_vp)
{
    // reconstruct position from depth
    vec4 ndc_pos = vec4(2.0 * coord - 1, depth, 1);
    vec4 world_space_pos = current_inverse_vp * ndc_pos;
    vec3 position = world_space_pos.xyz / world_space_pos.w;
    vec4 prev_ndc = previous_vp * vec4(position, 1.0);
    return 0.5 * prev_ndc.xy / prev_ndc.w + 0.5;
}

float linear_depth(float depth, float near, float far)
{
//    return near * far / (far + depth * (near - far));
    return (2.0 * near) / (far + near - depth * (far - near));
}

vec3 luma_weight(vec3 color)
{
    float luma = dot(LuminanceNTSC, color);
    return color / (1.0 + luma);
}

vec3 luma_weight_inverse(vec3 color)
{
    float luma = dot(LuminanceNTSC, color);
    return color / (1.0 - luma);
}

// note: clips towards aabb center + p.w
vec3 clip_aabb(vec3 aabb_min,
               vec3 aabb_max,
               vec3 c)
{
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min);
    vec3 v_clip = c - p_clip;
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float max_unit = max(a_unit.x, max(a_unit.y, a_unit.z));
    return max_unit > 1.0 ? p_clip + v_clip / max_unit : c;
}

//! temporal anti-aliasing (TAA) routine
vec4 taa(vec2 in_coord,
         sampler2D sampler_color,
         sampler2D sampler_depth,
         sampler2D sampler_motion,
         sampler2D sampler_color_history,
         sampler2D sampler_depth_history,
         taa_ubo_t taa_settings)
{
//    vec2 coord = in_coord;
    vec2 coord = in_coord + 0.5 * taa_settings.sample_offset;

    // current
    vec4 color = texture(sampler_color, coord);
    color.rgb = luma_weight(color.rgb);

    float depth = texture(sampler_depth, coord).x;

    vec2 motion_delta = texture(sampler_motion, coord).xy;

    // previous
    vec2 history_coord = coord - motion_delta;
    vec3 history_color = luma_weight(texture(sampler_color_history, history_coord).rgb);
    float history_depth = texture(sampler_depth_history, history_coord).x;

    // accumulation/blend factor
    float alpha = 0.1;

    // out of bounds sampling
    if(any(lessThan(history_coord, vec2(0))) || any(greaterThan(history_coord, vec2(1)))){ alpha = 1.0; }

    // reject and/or rectify based on color
    vec2 texel = 1.0 / textureSize(sampler_color, 0);
    vec3 min_color_0 = color.rgb;
    vec3 min_color_1 = color.rgb;
    vec3 max_color_0 = color.rgb;
    vec3 max_color_1 = color.rgb;

    // construct an averaged 3x3 neighbourhood AABB (in YCoCg color-space!?)
    const vec2[4] n_cross = vec2[](vec2(-1, 0), vec2(1, 0), vec2(0, 1), vec2(0, -1));
    const vec2[4] n_3x3 = vec2[](vec2(-1), vec2(1), vec2(-1, 1), vec2(1, -1));

    // neighbourhood in a cross shape
    for(uint i = 0; i < 4; ++i)
    {
        vec3 c_0 = RGB2YCoCg * luma_weight(texture(sampler_color, coord + texel * n_cross[i]).rgb);
        min_color_0 = min(min_color_0, c_0);
        max_color_0 = max(min_color_0, c_0);

        vec3 c_1 = RGB2YCoCg * luma_weight(texture(sampler_color, coord + texel * n_3x3[i]).rgb);
        min_color_1 = min(min_color_1, c_1);
        max_color_1 = max(min_color_1, c_1);

        depth = min(depth, texture(sampler_depth, coord + texel * n_cross[i]).x);
        depth = min(depth, texture(sampler_depth, coord + texel * n_3x3[i]).x);

        history_depth = min(history_depth, texture(sampler_depth_history, history_coord + texel * n_cross[i]).x);
        history_depth = min(history_depth, texture(sampler_depth_history, history_coord + texel * n_3x3[i]).x);
    }
    vec3 min_color = 0.5 * (min_color_0 + min_color_1);
    vec3 max_color = 0.5 * (max_color_0 + max_color_1);

    float depth_delta = abs(linear_depth(depth, taa_settings.near, taa_settings.far) -
        linear_depth(history_depth, taa_settings.near, taa_settings.far));

    // reject based on depth
    const float depth_eps = 5.0e-3; // 0.00000006 = 1.0 / (1 << 24)
    float depth_reject = depth_delta > depth_eps ? 0.8 : 0.0;
    alpha += depth_reject;

    // clip history against AABB
    vec3 history_ycocg = RGB2YCoCg * history_color;
    vec3 rectified_color = clip_aabb(min_color, max_color, history_ycocg);
    rectified_color.r = history_ycocg.r;
    rectified_color = YCoCg2RGB * rectified_color;

    alpha = clamp(alpha, 0.0, 1.0);

    history_color = mix(history_color, rectified_color, 1.0);

    color.rgb = mix(history_color, color.rgb, alpha);
    color.rgb = luma_weight_inverse(color.rgb);

//    return mix(history_color, color, alpha);
//    return mix(vec3(1, 0, 0), color, alpha);
    return color;
}