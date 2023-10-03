#ifndef UTILS_PROCEDURAL_ENVIRONMENT_GLSL
#define UTILS_PROCEDURAL_ENVIRONMENT_GLSL

// low-life + neutral environment-light
vec3 environment_white(vec3 direction)
{
    vec3 col_sky = vec3(1.3), col_horizon = vec3(.6), col_ground = vec3(.1);
    return direction.y > 0 ? mix(col_horizon, col_sky, direction.y) :
    mix(col_horizon, col_ground, -direction.y);
}

struct procedural_sky_params_t
{
    vec3 direction_to_light;
    float angular_size_of_light;

    vec3 light_color;
    float glow_size;

    vec3 sky_color;
    float glow_intensity;

    vec3 horizon_color;
    float horizon_size;

    vec3 ground_color;
    float glow_sharpness;

    vec3 direction_up;
    int pad;
};

vec3 procedural_sky(procedural_sky_params_t params, vec3 direction, float angular_px_size)
{
    float elevation = asin(clamp(dot(direction, params.direction_up), -1.0, 1.0));
    float top = smoothstep(0.f, params.horizon_size, elevation);
    float bottom = smoothstep(0.f, params.horizon_size, -elevation);
    vec3 environment = mix(mix(params.horizon_color, params.ground_color, bottom), params.sky_color, top);

    float angle_to_light = acos(clamp(dot(direction, params.direction_to_light), 0.0, 1.0));
    float half_angular_size = params.angular_size_of_light * 0.5;
    float light_intensity = clamp(1.0 - smoothstep(half_angular_size - angular_px_size * 2,
                                                   half_angular_size + angular_px_size * 2, angle_to_light), 0.0, 1.0);
    light_intensity = pow(light_intensity, 4.0);
    float glow_input = clamp(2.0 * (1.0 - smoothstep(half_angular_size - params.glow_size,
                                                     half_angular_size + params.glow_size, angle_to_light)), 0.0, 1.0);
    float glow_intensity = params.glow_intensity * pow(glow_input, params.glow_sharpness);
    vec3 light = max(light_intensity, glow_intensity) * params.light_color;
    return environment + light;
}

#endif //UTILS_PROCEDURAL_ENVIRONMENT_GLSL