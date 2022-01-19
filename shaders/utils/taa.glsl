#include "color_ycc.glsl"

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

//! temporal anti-aliasing (TAA) routine
vec3 taa(vec2 sample_coord,
         sampler2D sampler_color,
         sampler2D sampler_depth,
         sampler2D sampler_motion,
         sampler2D sampler_color_history,
         sampler2D sampler_depth_history,
         float z_near,
         float z_far)
{
    // current
    vec3 color = texture(sampler_color, sample_coord).rgb;
    float depth = texture(sampler_depth, sample_coord).x;

    // previous
    vec2 history_coord = sample_coord - texture(sampler_motion, sample_coord).xy;
    vec3 history_color = texture(sampler_color_history, history_coord).rgb;
    float history_depth = texture(sampler_depth_history, history_coord).x;

    // accumulation/blend factor
    float alpha = 0.1;

    // out of bounds sampling
    if(any(lessThan(history_coord, vec2(0))) || any(greaterThan(history_coord, vec2(1)))){ alpha = 1.0; }

    // reject based on depth
    float depth_delta = abs(linear_depth(depth, z_near, z_far) - linear_depth(history_depth, z_near, z_far));
    const float depth_eps = 5.0e-3; // 0.00000006 = 1.0 / (1 << 24)
    alpha = depth_delta > depth_eps ? 1.0 : alpha;

    // reject and/or rectify based on color
    vec2 texel = 1.0 / textureSize(sampler_color, 0);
    vec3 min_color = vec3(1);
    vec3 max_color = vec3(0);

    // construct a 3x3 neighbourhood AABB (in YCoCg color-space!?)
    for(int y = -1; y <= 1; ++y)
    {
        for(int x = -1; x <= 1; ++x)
        {
            vec3 c = texture(sampler_color, sample_coord + texel * vec2(x, y)).rgb;
            min_color = min(min_color, c);
            max_color = max(max_color, c);
        }
    }

    // clamp history against AABB
    history_color = clamp(history_color, min_color, max_color);

    //    return mix(vec3(1, 0, 0), color, alpha);
    alpha = clamp(alpha, 0.0, 1.0);
    return mix(history_color, color, alpha);
}