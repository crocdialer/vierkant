#ifndef UTILS_TAA_GLSL
#define UTILS_TAA_GLSL

#include "color_ycc.glsl"
#include "camera.glsl"

struct aabb_t
{
    vec3 min, max;
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
    // reverse+infite z
    return (near / depth - near) / (far - near);
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

aabb_t neighbourhood_variance(sampler2D tex, vec2 coord)
{
    const vec2 texel = 1.0 / textureSize(tex, 0);

    aabb_t ret;

    // construct the 3x3 neighbourhood variance (in YCoCg color-space)
    vec3 neighbourhood[9];
    uint n = 0;
    vec3 sum = vec3(0);

    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            vec3 c = RGB2YCoCg * luma_weight(texture(tex, coord + texel * vec2(x, y)).rgb);
            sum += c;
            neighbourhood[n++] = c;
        }
    }
    vec3 mean = sum / 9.0;
    vec3 sq_sum = vec3(0);

    for(uint i = 0; i < 9; ++i)
    {
        vec3 diff = neighbourhood[i] - mean;
        sq_sum += diff * diff;
    }
    vec3 stddev = sqrt(sq_sum / 9.0);

    float variance_factor = 1.25;
    stddev *= variance_factor;

    ret.min = mean - stddev;
    ret.max = mean + stddev;
    return ret;
}

//! temporal anti-aliasing (TAA) routine
vec4 taa(vec2 in_coord,
         sampler2D sampler_color,
         sampler2D sampler_depth,
         sampler2D sampler_motion,
         sampler2D sampler_color_history,
         sampler2D sampler_depth_history,
         camera_t taa_settings)
{
    vec2 coord = in_coord - taa_settings.sample_offset;
    vec2 texSize = textureSize(sampler_color, 0);
    vec2 texel = 1.0 / texSize;

    // current
    vec4 color = texture(sampler_color, coord);
    color.rgb = luma_weight(color.rgb);

    // find min depth in 3x3 neighbourhood
    float min_depth = texelFetch(sampler_depth, ivec2(coord * texSize), 0).x;
    vec2 min_depth_coord = coord;

    const vec2[4] diagonal_cross = vec2[](vec2(-1), vec2(1), vec2(-1, 1), vec2(1, -1));

    for(uint o = 0; o < 4; ++o)
    {
        vec2 d_coord = coord + texel * diagonal_cross[o];
        float d = texelFetch(sampler_depth, ivec2(d_coord * texSize), 0).x;

        if(d > min_depth)
        {
            min_depth = d;
            min_depth_coord = d_coord;
        }
    }

    // max motion-delta in screenspace
    const float max_motion_delta = 0.15;
    vec2 motion_delta = min(texture(sampler_motion, min_depth_coord).xy, max_motion_delta);

    // previous
    vec2 history_coord = in_coord - motion_delta;
    vec3 history_color = luma_weight(texture(sampler_color_history, history_coord).rgb);

    float history_depth = 1.0;//texelFetch(sampler_depth_history, ivec2(history_coord * texSize), 0).x;
    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            vec2 d_coord = history_coord + texel * vec2(x, y);
            float d = texelFetch(sampler_depth_history, ivec2(d_coord * texSize), 0).x;
            history_depth = min(history_depth, d);
        }
    }

    // accumulation/blend factor
    float alpha = 0.1;

    // motion adjustment to alpha
    alpha += min(0.02 * length(motion_delta / texel), 0.25);

    // out of bounds sampling
    if(any(lessThan(history_coord, vec2(0))) || any(greaterThan(history_coord, vec2(1)))){ alpha = 1.0; }

    float depth_delta = abs(linear_depth(min_depth, taa_settings.near, taa_settings.far) -
        linear_depth(history_depth, taa_settings.near, taa_settings.far));

    // reject based on depth
    const float depth_eps = 5.0e-3;
    float depth_reject = depth_delta > depth_eps ? .1 : 0.0;
    alpha += depth_reject;

    // clip history against AABB
    aabb_t variance_aabb = neighbourhood_variance(sampler_color, coord);
    vec3 history_ycocg = RGB2YCoCg * history_color;
    vec3 rectified_color = clip_aabb(variance_aabb.min, variance_aabb.max, history_ycocg);
    rectified_color = YCoCg2RGB * rectified_color;

//    vec3 rect_diff = history_color - rectified_color;
//    float rect_len2 = dot(rect_diff, rect_diff);

    alpha = clamp(alpha, 0.0, 1.0);

    history_color = mix(history_color, rectified_color, 1.0);

    color.rgb = mix(history_color, color.rgb, alpha);
    color.rgb = luma_weight_inverse(color.rgb);

//    return vec4(vec3(linear_depth(history_depth, taa_settings.near, taa_settings.far)), 1.0);
//    return mix(color, vec4(1, 0, 0, 1), depth_reject);
    return max(color, 0);
}

#endif // UTILS_TAA_GLSL